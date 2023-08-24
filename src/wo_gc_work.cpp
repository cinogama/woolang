#include "wo_vm.hpp"
#include "wo_memory.hpp"

#include <thread>
#include <chrono>
#include <list>

// PARALLEL-GC SUPPORT:
/*
GC will do following work to instead of old-gc:
0. Get all vm(s) for GC.
1. Stop the world;
2. for all vm, mark their root object, for global-space, only mark once.
3. Mark a sign to let all gc-unit know that now is continuing marking, any new and read gc-unit will make gc-unit gray,
4. Resume the world;
5. Do Tri-colors GC.
6. All markable unit is marked now, Clear sign (which marked in stage 3).
7. Recycle all white unit.
*/

#define WO_GC_FORCE_STOP_WORLD false
#define WO_GC_DEBUG false

namespace wo
{
    // A very simply GC system, just stop the vm, then collect inform
    namespace gc
    {
        uint16_t                    _gc_round_count = 0;
        constexpr uint16_t          _gc_work_thread_count = 4;
        constexpr uint16_t          _gc_max_count_to_move_young_to_old = 15;

        std::atomic_bool            _gc_stop_flag = false;
        std::thread                 _gc_scheduler_thread;
        std::condition_variable     _gc_work_cv;
        std::mutex                  _gc_work_mx;

        std::atomic_flag            _gc_immediately = {};

        uint32_t                    _gc_immediately_edge = 250000;
        uint32_t                    _gc_stop_the_world_edge = _gc_immediately_edge * 5;

        std::atomic_size_t _gc_scan_vm_index;
        size_t _gc_scan_vm_count;
        std::atomic<vmbase**> _gc_vm_list;

        std::atomic_bool _gc_is_marking = false;
        std::atomic_bool _gc_is_recycling = false;

        bool _gc_stopping_world_gc = WO_GC_FORCE_STOP_WORLD;

        bool gc_is_marking()
        {
            return _gc_is_marking.load();
        }
        bool gc_is_recycling()
        {
            return _gc_is_recycling.load();
        }

        vmbase* _get_next_mark_vm(vmbase::vm_type* out_vm_type)
        {
            size_t id = _gc_scan_vm_index++;
            if (id < _gc_scan_vm_count)
            {
                *out_vm_type = _gc_vm_list.load()[id]->virtual_machine_type;
                return _gc_vm_list.load()[id];
            }

            return nullptr;
        }

        std::list<gcbase*> _gc_gray_unit_lists[_gc_work_thread_count];
        std::unordered_map<vmbase*, std::list<gcbase*>> _gc_vm_gray_unit_lists;

        void gc_mark_unit_as_gray(std::list<gcbase*>* worklist, gcbase* unit)
        {
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::no_mark)
            {
                unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::self_mark);
                worklist->push_back(unit);
            }
        }

        void gc_mark_unit_as_black(std::list<gcbase*>* worklist, gcbase* unit)
        {
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::full_mark)
                return;

            unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::full_mark);

            wo::gcbase::gc_read_guard g1(unit);

            gcbase::memo_unit* memo = unit->pick_memo();
            while (memo)
            {
                auto* curmemo = memo;
                memo = memo->last;

                gc_mark_unit_as_gray(worklist, curmemo->gcunit);

                delete curmemo;
            }

            if (array_t* wo_arr = dynamic_cast<array_t*>(unit))
            {
                for (auto& val : *wo_arr)
                {
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(worklist, gcunit_addr);
                }
            }
            else if (dict_t* wo_map = dynamic_cast<dict_t*>(unit))
            {
                for (auto& [key, val] : *wo_map)
                {
                    if (gcbase* gcunit_addr = key.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(worklist, gcunit_addr);
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(worklist, gcunit_addr);
                }
            }
            else if (gchandle_t* wo_gchandle = dynamic_cast<gchandle_t*>(unit))
            {
                if (gcbase* gcunit_addr = wo_gchandle->holding_value.get_gcunit_with_barrier())
                    gc_mark_unit_as_gray(worklist, gcunit_addr);
            }
            else if (closure_t* wo_closure = dynamic_cast<closure_t*>(unit))
            {
                for (uint16_t i = 0; i < wo_closure->m_closure_args_count; ++i)
                    if (gcbase* gcunit_addr = wo_closure->m_closure_args[i].get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(worklist, gcunit_addr);
            }
            else if (struct_t* wo_struct = dynamic_cast<struct_t*>(unit))
            {
                for (uint16_t i = 0; i < wo_struct->m_count; ++i)
                    if (gcbase* gcunit_addr = wo_struct->m_values[i].get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(worklist, gcunit_addr);
            }
        }

        class _gc_mark_thread_groups
        {
            std::thread _m_gc_mark_threads[_gc_work_thread_count];
            std::atomic_flag _m_gc_begin_flags[_gc_work_thread_count];

            std::mutex _m_gc_begin_mx;
            std::condition_variable _m_gc_begin_cv;

            std::mutex _m_gc_end_mx;
            std::condition_variable _m_gc_end_cv;
            std::atomic_size_t _m_gc_mark_end_count;

            std::atomic_bool _m_worker_enabled;

            static void _gcmarker_thread_work(_gc_mark_thread_groups* self, size_t worker_id)
            {
#ifdef WO_PLATRORM_OS_WINDOWS
                SetThreadDescription(GetCurrentThread(), L"wo_gc_marker");
#endif
                do
                {
                    do
                    {
                        std::unique_lock ug1(self->_m_gc_begin_mx);
                        self->_m_gc_begin_cv.wait(ug1, [&]()->bool {
                            return !self->_m_gc_begin_flags[worker_id].test_and_set()
                                || !self->_m_worker_enabled;
                            });
                        if (!self->_m_worker_enabled)
                            return;

                    } while (false);

                    vmbase* marking_vm = nullptr;
                    vmbase::vm_type vm_type;
                    while ((marking_vm = _get_next_mark_vm(&vm_type)))
                    {
                        if (vm_type == vmbase::vm_type::GC_DESTRUCTOR)
                        {
                            if (auto env = marking_vm->env)
                            {
                                // If current gc-vm is orphan, skip marking global value.
                                if (env->_running_on_vm_count > 1)
                                {
                                    // Any code context only have one GC_DESTRUCTOR, here to mark global space.
                                    auto* global_and_const_values = env->constant_global_reg_rtstack;

                                    // Skip all constant, all constant cannot contain gc-type value beside no-gc-string.
                                    for (size_t cgr_index = env->constant_value_count;
                                        cgr_index < env->constant_and_global_value_takeplace_count;
                                        cgr_index++)
                                    {
                                        auto* global_val = global_and_const_values + cgr_index;

                                        gcbase* gcunit_address = global_val->get_gcunit_with_barrier();
                                        if (gcunit_address)
                                            gc_mark_unit_as_gray(&_gc_gray_unit_lists[worker_id], gcunit_address);
                                    }
                                }
                            }
                        }
                        else
                            mark_vm(marking_vm, worker_id);
                    }

                    if (_gc_work_thread_count == ++self->_m_gc_mark_end_count)
                    {
                        // All mark thread end, notify..
                        std::lock_guard g1(self->_m_gc_end_mx);
                        self->_m_gc_end_cv.notify_all();
                    }
                    ////////////////////////////////////////////////////////////////
                    // Do gc fullmark here
                    do
                    {
                        std::unique_lock ug1(self->_m_gc_begin_mx);
                        self->_m_gc_begin_cv.wait(ug1, [&]()->bool {
                            return !self->_m_gc_begin_flags[worker_id].test_and_set()
                                || !self->_m_worker_enabled;
                            });
                        if (!self->_m_worker_enabled)
                            return;

                    } while (false);

                    std::list<gcbase*> graylist;
                    while (!_gc_gray_unit_lists[worker_id].empty())
                    {
                        graylist.clear();
                        graylist.swap(_gc_gray_unit_lists[worker_id]);
                        for (auto* markingunit : graylist)
                        {
                            gc_mark_unit_as_black(&_gc_gray_unit_lists[worker_id], markingunit);
                        }
                    }

                    if (_gc_work_thread_count == ++self->_m_gc_mark_end_count)
                    {
                        // All mark thread end, notify..
                        std::lock_guard g1(self->_m_gc_end_mx);
                        self->_m_gc_end_cv.notify_all();
                    }

                } while (true);
            }
        public:
            _gc_mark_thread_groups()
            {
                start();
            }
            ~_gc_mark_thread_groups()
            {
                stop();
            }
            static _gc_mark_thread_groups& instancce()
            {
                static _gc_mark_thread_groups g;
                return g;
            }
        public:
            void stop()
            {
                if (_m_worker_enabled)
                {
                    std::lock_guard g1(_m_gc_begin_mx);
                    _m_worker_enabled = false;
                    _m_gc_begin_cv.notify_all();
                }

                for (size_t id = 0; id < _gc_work_thread_count; ++id)
                    _m_gc_mark_threads[id].join();
            }
            void start()
            {
                if (!_m_worker_enabled)
                {
                    _m_worker_enabled = true;
                    for (size_t id = 0; id < _gc_work_thread_count; ++id)
                    {
                        _m_gc_begin_flags[id].test_and_set(); // make sure gcmarkers donot work at begin.
                        _m_gc_mark_threads[id] = std::move(std::thread(_gcmarker_thread_work, this, id));
                    }
                }
            }

            void launch_round_of_mark()
            {
                _m_gc_mark_end_count = 0;
                do
                {
                    std::lock_guard g1(_m_gc_begin_mx);
                    for (size_t id = 0; id < _gc_work_thread_count; ++id)
                        _m_gc_begin_flags[id].clear();

                    _m_gc_begin_cv.notify_all();
                } while (false);


                do
                {
                    std::unique_lock ug1(_m_gc_end_mx);
                    _m_gc_end_cv.wait(ug1, [this]() {return _m_gc_mark_end_count == _gc_work_thread_count; });
                } while (false);
            }
        };

        void mark_nogc_child(gcbase* picked_list, size_t worker_id)
        {
            while (picked_list)
            {
                if (picked_list->gc_type == gcbase::gctype::no_gc
                    && gcbase::gcmarkcolor::no_mark == picked_list->gc_marked(_gc_round_count))
                    gc_mark_unit_as_gray(&_gc_gray_unit_lists[worker_id], picked_list);

                picked_list = picked_list->last;
            }
        }

        void check_and_move_edge_to_edge(gcbase* picked_list,
            wo::atomic_list<wo::gcbase>* origin_list,
            wo::atomic_list<wo::gcbase>* aim_edge,
            wo::gcbase::gctype aim_gc_type,
            uint16_t max_count)
        {
            while (picked_list)
            {
                auto* last = picked_list->last;

                if (picked_list->gc_type != gcbase::gctype::no_gc &&
                    gcbase::gcmarkcolor::no_mark == picked_list->gc_marked(_gc_round_count))
                {
                    // Unit was not marked, delete it
                    picked_list->~gcbase();
                    free64(picked_list);
                } // ~
                else
                {
                    if (!origin_list || ((picked_list->gc_mark_alive_count > max_count) && aim_edge))
                        // It lived so long, move it to old_edge.
                        aim_edge->add_one(picked_list);
                    else
                        // Add it back
                        origin_list->add_one(picked_list);

                    // ATTENTION: DONOT OVERWRITE NO_GC FLAG!
                    if (picked_list->gc_type != gcbase::gctype::no_gc)
                        picked_list->gc_type = aim_gc_type;
                }

                picked_list = last;
            }
        }

        void _gc_work_list()
        {
#ifdef WO_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"wo_gc_main");
#endif
            // Pick all gcunit before 1st mark begin.
            // It means all unit alloced when marking will be skiped to free.
            auto* young_list = gcbase::young_age_gcunit_list.pick_all();
            auto* old_list = gcbase::old_age_gcunit_list.pick_all();

            std::vector<vmbase*> gc_marking_vmlist, self_marking_vmlist, time_out_vmlist;

            // 0. get current vm list, set stop world flag to TRUE:
            do
            {
                std::shared_lock sg1(vmbase::_alive_vm_list_mx); // Lock alive vm list, block new vm create.
                _gc_round_count++;

                _gc_is_marking = true;

                // 0. Prepare vm gray unit list
                if (!_gc_stopping_world_gc)
                {
                    _gc_vm_gray_unit_lists.clear();
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            _gc_vm_gray_unit_lists[vmimpl] = {};
                    }

                    // 1. Interrupt all vm as GC_INTERRUPT, let all vm hang-up
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            wo_asure(vmimpl->interrupt(vmbase::GC_INTERRUPT));

                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                        {
                            switch (vmimpl->wait_interrupt(vmbase::GC_INTERRUPT))
                            {
                            case vmbase::interrupt_wait_result::TIMEOUT:
                                // HANGUP_INTERRUPT may failed, ignore and handle it later.
                            case vmbase::interrupt_wait_result::ACCEPT:
                                // Current vm is self marking...
                                self_marking_vmlist.push_back(vmimpl);
                                continue;
                            case vmbase::interrupt_wait_result::LEAVED:
                                break;
                            }
                        }

                        // Current vm will be mark by gc-work-thread.
                        gc_marking_vmlist.push_back(vmimpl);
                    }
                }
                else
                {
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        // NOTE: Let vm hang up for stop the world GC
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            wo_asure(vmimpl->interrupt(vmbase::vm_interrupt_type::HANGUP_INTERRUPT));
                    }

                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            vmimpl->wait_interrupt(vmbase::HANGUP_INTERRUPT);

                        // Current vm will be mark by gc-work-thread.
                        gc_marking_vmlist.push_back(vmimpl);
                    }
                }

                // 2. Mark all unit in vm's stack, register, global(only once)
                _gc_scan_vm_count = gc_marking_vmlist.size();

                _gc_vm_list.store(gc_marking_vmlist.data());
                _gc_scan_vm_index.store(0);

                // 3. Start GC Worker for first marking        
                _gc_mark_thread_groups::instancce().launch_round_of_mark();

                // 4. Wake up all hanged vm.
                if (!_gc_stopping_world_gc)
                {
                    for (auto* vmimpl : gc_marking_vmlist)
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            if (!vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                                vmimpl->wakeup();

                    // 5. Wait until all self-marking vm work finished
                    for (auto* vmimpl : self_marking_vmlist)
                    {
                        auto self_mark_gc_state = vmimpl->wait_interrupt(vmbase::GC_INTERRUPT);
                        wo_assert(vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL);
                        wo_assert(self_mark_gc_state != vmbase::interrupt_wait_result::LEAVED);
                        if (self_mark_gc_state == vmbase::interrupt_wait_result::TIMEOUT)
                        {
                            // Current vm is still structed, let it hangup and mark it here.
                            if (vmimpl->interrupt(vmbase::vm_interrupt_type::HANGUP_INTERRUPT))
                            {
                                mark_vm(vmimpl, SIZE_MAX);

                                if (!vmimpl->clear_interrupt(vmbase::vm_interrupt_type::HANGUP_INTERRUPT))
                                    vmimpl->wakeup();
                            }
                        }
                        wo_asure(vmimpl->wait_interrupt(vmbase::HANGUP_INTERRUPT) == vmbase::interrupt_wait_result::ACCEPT);
                    }

                    // 6. Merge gray lists.
                    size_t worker_id_counter = 0;
                    for (auto& [_vm, gray_list] : _gc_vm_gray_unit_lists)
                    {
                        auto& worker_gray_list = _gc_gray_unit_lists[worker_id_counter++ % _gc_work_thread_count];
                        worker_gray_list.insert(worker_gray_list.end(), gray_list.begin(), gray_list.end());
                    }
                }

            } while (0);
            // just full gc:

            // Mark all no_gc_object
            // mark_nogc_child(eden_list, 0 % _gc_work_thread_count);
            // mark_nogc_child(young_list, 1 % _gc_work_thread_count);
            // mark_nogc_child(old_list, 2 % _gc_work_thread_count);

#if WO_GC_DEBUG
            wo::wo_stdout << "================================================" << wo::wo_endl;
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_RST " A round of gc launched." << wo::wo_endl;
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_RST " Before GC Free:" << wo::wo_endl;

            size_t gcunit_count = 0;
            auto* elem = young_list;
            while (elem)
            {
                ++gcunit_count;
                elem = elem->last;
            }
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_HIY " Young: " ANSI_RST << gcunit_count << wo::wo_endl;

            gcunit_count = 0;
            elem = old_list;
            while (elem)
            {
                ++gcunit_count;
                elem = elem->last;
            }
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_HIM " Old: " ANSI_RST << gcunit_count << wo::wo_endl;
#endif

            // 4. OK, Continue mark gray to black
            _gc_mark_thread_groups::instancce().launch_round_of_mark();

            // Marking finished.
            _gc_is_marking = false;
            _gc_is_recycling = true;

            // 5. OK, All unit has been marked. reduce gcunits
            check_and_move_edge_to_edge(old_list, &gcbase::old_age_gcunit_list, nullptr, gcbase::gctype::old, UINT16_MAX);
            check_and_move_edge_to_edge(young_list, &gcbase::young_age_gcunit_list, &gcbase::old_age_gcunit_list, gcbase::gctype::old, _gc_max_count_to_move_young_to_old);

#if WO_GC_DEBUG
            wo::wo_stdout << "------------------------------------------------" << wo::wo_endl;
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_RST " After GC Free:" << wo::wo_endl;

            gcunit_count = 0;
            elem = gcbase::young_age_gcunit_list.last_node;
            while (elem)
            {
                ++gcunit_count;
                elem = elem->last;
            }
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_HIY " Young: " ANSI_RST << gcunit_count << wo::wo_endl;

            gcunit_count = 0;
            elem = gcbase::old_age_gcunit_list.last_node;
            while (elem)
            {
                ++gcunit_count;
                elem = elem->last;
            }
            wo::wo_stdout << ANSI_HIG "[GCDEBUG]" ANSI_HIM " Old: " ANSI_RST << gcunit_count << wo::wo_endl;
            wo::wo_stdout << "================================================" << wo::wo_endl;
#endif

            // 6. Remove orpho vm
            std::list<vmbase*> need_destruct_gc_destructor_list;

            do
            {
                std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                for (auto* vmimpl : vmbase::_alive_vm_list)
                {
                    if (vmimpl->virtual_machine_type == vmbase::vm_type::GC_DESTRUCTOR && vmimpl->env->_running_on_vm_count == 1)
                    {
                        // Assure vm's stack if empty
                        wo_assert(vmimpl->sp == vmimpl->bp && vmimpl->bp == vmimpl->stack_mem_begin);

                        // If there is no instance of gc-handle which may use library loaded in env,
                        // then free the gc destructor vm.
                        if (0 == vmimpl->env->_created_destructable_instance_count.load(std::memory_order::memory_order_relaxed))
                            need_destruct_gc_destructor_list.push_back(vmimpl);
                    }
                }

                if (_gc_stopping_world_gc)
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            if (!vmimpl->clear_interrupt(vmbase::HANGUP_INTERRUPT))
                                vmimpl->wakeup();

            } while (0);


            for (auto* destruct_vm : need_destruct_gc_destructor_list)
                delete destruct_vm;

            _gc_is_recycling = false;

            // All jobs done.
            _gc_stopping_world_gc = WO_GC_FORCE_STOP_WORLD;
        }

        void _gc_main_thread()
        {
            do
            {
                _gc_work_list();

                do
                {
                    std::unique_lock ug1(_gc_work_mx);
                    for (size_t i = 0; i < 100; ++i)
                    {
                        using namespace std;
                        bool breakout = false;

                        if (gcbase::gc_new_count > _gc_immediately_edge)
                        {
                            if (gcbase::gc_new_count > _gc_stop_the_world_edge)
                            {
                                _gc_stopping_world_gc = true;
                                gcbase::gc_new_count -= _gc_stop_the_world_edge;
                            }
                            else
                                gcbase::gc_new_count -= _gc_immediately_edge;
                            break;
                        }

                        _gc_work_cv.wait_for(ug1, 0.1s, [&]() {
                            if (_gc_stop_flag || !_gc_immediately.test_and_set())
                                breakout = true;
                            return breakout;
                            });

                        if (breakout)
                            break;
                    }
                } while (false);

            } while (!_gc_stop_flag);
        }

        void gc_start()
        {
            _gc_stop_flag = false;
            _gc_immediately.test_and_set();
            _gc_scheduler_thread = std::move(std::thread(_gc_main_thread));
        }

        void mark_vm(vmbase* marking_vm, size_t worker_id)
        {
            wo_assert(marking_vm->virtual_machine_type == vmbase::vm_type::NORMAL);
            // Mark stack, reg, 

            bool is_self_marking = worker_id == SIZE_MAX;
            auto* worklist = is_self_marking
                ? &_gc_vm_gray_unit_lists[marking_vm]
                : &_gc_gray_unit_lists[worker_id]
                ;

            if (auto env = marking_vm->env)
            {
                // walk thorgh regs.
                for (size_t reg_index = 0;
                    reg_index < env->real_register_count;
                    reg_index++)
                {
                    auto self_reg_walker = marking_vm->register_mem_begin + reg_index;

                    gcbase* gcunit_address = self_reg_walker->get_gcunit_with_barrier();
                    if (gcunit_address)
                        gc_mark_unit_as_gray(worklist, gcunit_address);

                }

                // walk thorgh stack.
                for (auto* stack_walker = marking_vm->stack_mem_begin/*env->stack_begin*/;
                    marking_vm->sp < stack_walker;
                    stack_walker--)
                {
                    auto stack_val = stack_walker;

                    gcbase* gcunit_address = stack_val->get_gcunit_with_barrier();
                    if (gcunit_address)
                        gc_mark_unit_as_gray(worklist, gcunit_address);
                }
            }
        }

    } // END NAME SPACE gc
    
    void gcbase::add_memo(const value* val)
    {
        if (auto* mem = val->get_gcunit_with_barrier())
        {
            if (gcbase::gcmarkcolor::no_mark != mem->gc_marked(gc::_gc_round_count))
                return;

            memo_unit* new_memo = new memo_unit{ mem, m_memo.load() };
            while (!m_memo.compare_exchange_weak(new_memo->last, new_memo));
        }
    }

    bool gc_handle_base_t::close()
    {
        if (!has_been_closed_af.test_and_set())
        {
            wo_assert(has_been_closed == false);
            has_been_closed = true;
            if (destructor != nullptr)
            {
                wo_assert(gc_vm != nullptr);

                destructor(holding_handle);
                reinterpret_cast<wo::vmbase*>(gc_vm)->dec_destructable_instance_count();
            }
            return true;
        }
        return false;
    }

}

void wo_gc_immediately()
{
    std::lock_guard g1(wo::gc::_gc_work_mx);
    wo::gc::_gc_immediately.clear();
    wo::gc::_gc_work_cv.notify_one();
}

void wo_gc_stop()
{
    do
    {
        std::lock_guard g1(wo::gc::_gc_work_mx);
        wo::gc::_gc_stop_flag = true;
        wo::gc::_gc_work_cv.notify_one();
    } while (false);

    wo::gc::_gc_scheduler_thread.join();
}
