#define _CRT_SECURE_NO_WARNINGS
#include "wo_vm.hpp"
#include "wo_memory.hpp"

#include <thread>
#include <chrono>
#include<list>

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

namespace wo
{
    void gcbase::add_memo(const value* val)
    {
        // TODO: IF HAS BEEN FULL-MAKRED, SKIP MEMORY OPERATION
        if (auto* mem = val->get_gcunit_with_barrier())
        {
            memo_unit* last_memo = m_memo;
            memo_unit* new_memo = new memo_unit{ mem, nullptr };
            do
            {
                new_memo->last = last_memo;

            } while (!m_memo.compare_exchange_weak(last_memo, new_memo));
        }
    }

    // A very simply GC system, just stop the vm, then collect inform
// #define WO_GC_DEBUG
    namespace gc
    {
        uint16_t                    _gc_round_count = 0;
        constexpr uint16_t          _gc_work_thread_count = 4;
        constexpr uint16_t          _gc_max_count_to_move_young_to_old = 5;

        std::atomic_bool            _gc_stop_flag = false;
        std::thread                 _gc_scheduler_thread;
        std::condition_variable     _gc_work_cv;
        std::mutex                  _gc_work_mx;

        std::atomic_flag            _gc_immediately = {};

        uint32_t                    _gc_immediately_edge = 50000;
        uint32_t                    _gc_stop_the_world_edge = _gc_immediately_edge * 5;

        std::atomic_size_t _gc_scan_vm_index;
        std::atomic_size_t _gc_scan_vm_count;
        std::atomic<vmbase**> _gc_vm_list;

        std::atomic_bool _gc_is_marking = false;
        bool _gc_stopping_world_gc = false;

        bool gc_is_marking()
        {
            return _gc_is_marking.load();
        }

        vmbase* _get_next_mark_vm(vmbase::vm_type* out_vm_type)
        {
            size_t id = _gc_scan_vm_index++;
            if (id < _gc_scan_vm_count.load())
            {
                *out_vm_type = _gc_vm_list.load()[id]->virtual_machine_type;
                return _gc_vm_list.load()[id];
            }

            return nullptr;
        }

        std::list<gcbase*> _gc_gray_unit_lists[_gc_work_thread_count];

        void gc_mark_unit_as_gray(size_t workerid, gcbase* unit)
        {
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::no_mark)
            {
                unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::self_mark);

                // TODO: Optmize here, donot use new.
                _gc_gray_unit_lists[workerid].push_back(unit);
            }
        }

        void gc_mark_unit_as_black(size_t workerid, gcbase* unit)
        {
            // TODO: Make 'gc_mark' atomicable
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::full_mark)
                return;

            unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::full_mark);

            wo::gcbase::gc_read_guard g1(unit);

            gcbase::memo_unit* memo = unit->pick_memo();
            while (memo)
            {
                auto* curmemo = memo;
                memo = memo->last;

                gc_mark_unit_as_gray(workerid, curmemo->gcunit);

                delete curmemo;
            }

            if (array_t* wo_arr = dynamic_cast<array_t*>(unit))
            {
                for (auto& val : *wo_arr)
                {
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(workerid, gcunit_addr);
                }
            }
            else if (mapping_t* wo_map = dynamic_cast<mapping_t*>(unit))
            {
                for (auto& [key, val] : *wo_map)
                {
                    if (gcbase* gcunit_addr = key.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(workerid, gcunit_addr);
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(workerid, gcunit_addr);
                }
            }
            else if (gchandle_t* wo_gchandle = dynamic_cast<gchandle_t*>(unit))
            {
                if (gcbase* gcunit_addr = wo_gchandle->holding_value.get_gcunit_with_barrier())
                    gc_mark_unit_as_gray(workerid, gcunit_addr);
            }
            else if (closure_t* wo_closure = dynamic_cast<closure_t*>(unit))
            {
                for (auto& captured : wo_closure->m_closure_args)
                {
                    if (gcbase* gcunit_addr = captured.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(workerid, gcunit_addr);
                }
            }
            else if (struct_t* wo_struct = dynamic_cast<struct_t*>(unit))
            {
                for (uint16_t i = 0; i < wo_struct->m_count; ++i)
                    if (gcbase* gcunit_addr = wo_struct->m_values[i].get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(workerid, gcunit_addr);
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
                        if (auto env = marking_vm->env)
                        {
                            if (vm_type == vmbase::vm_type::GC_DESTRUCTOR)
                            {
                                // Any code context only have one GC_DESTRUCTOR, here to mark global space.
                                for (size_t cgr_index = 0;
                                    cgr_index < env->constant_and_global_value_takeplace_count;
                                    cgr_index++)
                                {
                                    auto global_val = env->constant_global_reg_rtstack + cgr_index;

                                    gcbase* gcunit_address = global_val->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(worker_id, gcunit_address);
                                }
                            }
                            else
                            {
                                wo_assert(vm_type == vmbase::vm_type::NORMAL);
                                // Mark stack, reg, 

                                // walk thorgh regs.
                                for (size_t reg_index = 0;
                                    reg_index < env->real_register_count;
                                    reg_index++)
                                {
                                    auto self_reg_walker = marking_vm->register_mem_begin + reg_index;

                                    gcbase* gcunit_address = self_reg_walker->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(worker_id, gcunit_address);

                                }

                                // walk thorgh stack.
                                for (auto* stack_walker = marking_vm->stack_mem_begin/*env->stack_begin*/;
                                    marking_vm->sp < stack_walker;
                                    stack_walker--)
                                {
                                    auto stack_val = stack_walker;

                                    gcbase* gcunit_address = stack_val->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(worker_id, gcunit_address);
                                }
                            }
                        }
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
                            gc_mark_unit_as_black(worker_id, markingunit);
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
                    gc_mark_unit_as_gray(worker_id, picked_list);

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
                    picked_list->gc_type != gcbase::gctype::eden &&
                    gcbase::gcmarkcolor::no_mark == picked_list->gc_marked(_gc_round_count))
                {
                    // was not marked, delete it
                    // TODO: is map? if is map check it if need gc_destruct?

                    picked_list->~gcbase();
                    free64(picked_list);

                } // ~
                else
                {
                    //ATTENTION: A BUG CAUSED BY OVERWRITE NO_GC FLAG
                    //
                    // In gchandle, guard_value will be set aas 'no_gc' to make sure it destruct after
                    // gchandle, but in here, we overwrite gc_type, it will make guard_value destruct before
                    // gchandle in once gc-work.
                    // in this situation, gchandle destruct, and reset guard_value's gc-type. but memory has
                    // been reused. this written operation will write to wrong place.
                    //

                    if (!origin_list || ((picked_list->gc_type == gcbase::gctype::eden
                        || picked_list->gc_mark_alive_count > max_count) && aim_edge))
                    {
                        // gcbase::gc_write_guard gcwg1(picked_list);

                        // over count, move it to old_edge.
                        aim_edge->add_one(picked_list);

                        // DONOT OVERWRITE NO_GC FLAG!
                        if (picked_list->gc_type != gcbase::gctype::no_gc)
                            picked_list->gc_type = aim_gc_type;
                    }
                    else
                    {
                        // gcbase::gc_write_guard gcwg1(picked_list);

                        // add it back
                        origin_list->add_one(picked_list);

                        // DONOT OVERWRITE NO_GC FLAG!
                        if (picked_list->gc_type != gcbase::gctype::no_gc)
                            picked_list->gc_type = aim_gc_type;
                    }
                }

                picked_list = last;
            }
        }

        void _gc_work_list()
        {
#ifdef WO_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"wo_gc_main");
#endif

            // 0. get current vm list, set stop world flag to TRUE:
            do
            {
                std::shared_lock sg1(vmbase::_alive_vm_list_mx); // Lock alive vm list, block new vm create.
                _gc_round_count++;

                _gc_is_marking = true;

                // 1. Interrupt all vm as GC_INTERRUPT, let all vm hang-up
                for (auto* vmimpl : vmbase::_alive_vm_list)
                    if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                        vmimpl->interrupt(vmbase::GC_INTERRUPT);

                for (auto* vmimpl : vmbase::_alive_vm_list)
                    if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                        vmimpl->wait_interrupt(vmbase::GC_INTERRUPT);

                // 2. Mark all unit in vm's stack, register, global(only once)
                _gc_scan_vm_count.store(vmbase::_alive_vm_list.size());

                std::vector<vmbase*> vmlist(_gc_scan_vm_count.load());

                auto vm_index = vmbase::_alive_vm_list.begin();
                for (size_t i = 0; i < _gc_scan_vm_count.load(); ++i)
                    vmlist[i] = *(vm_index++);
                _gc_vm_list.store(vmlist.data());
                _gc_scan_vm_index.store(0);

                // 3. Start GC Worker for first marking        
                _gc_mark_thread_groups::instancce().launch_round_of_mark();

                if (!_gc_stopping_world_gc)
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            if (!vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                                vmimpl->wakeup();

            } while (0);
            // just full gc:
            auto* eden_list = gcbase::eden_age_gcunit_list.pick_all();
            auto* young_list = gcbase::young_age_gcunit_list.pick_all();
            auto* old_list = gcbase::old_age_gcunit_list.pick_all();

            // Mark all no_gc_object
            // mark_nogc_child(eden_list, 0 % _gc_work_thread_count);
            // mark_nogc_child(young_list, 1 % _gc_work_thread_count);
            // mark_nogc_child(old_list, 2 % _gc_work_thread_count);

            // 4. OK, Continue mark gray to black
            _gc_mark_thread_groups::instancce().launch_round_of_mark();

            // Marking finished.
            _gc_is_marking = false;

            // 5. OK, All unit has been marked. reduce gcunits
            check_and_move_edge_to_edge(old_list, &gcbase::old_age_gcunit_list, nullptr, gcbase::gctype::old, UINT16_MAX);
            check_and_move_edge_to_edge(young_list, &gcbase::young_age_gcunit_list, &gcbase::old_age_gcunit_list, gcbase::gctype::old, _gc_max_count_to_move_young_to_old);

            // Move all eden to young
            check_and_move_edge_to_edge(eden_list, nullptr, &gcbase::young_age_gcunit_list, gcbase::gctype::young, 0);

            // 6. Remove orpho vm
            std::list<vmbase*> need_destruct_gc_destructor_list;

            do
            {
                std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                for (auto* vmimpl : vmbase::_alive_vm_list)
                {
                    if (vmimpl->virtual_machine_type == vmbase::vm_type::GC_DESTRUCTOR && vmimpl->env->_running_on_vm_count == 1)
                    {
                        // assure vm's stack if empty
                        wo_assert(vmimpl->sp == vmimpl->bp && vmimpl->bp == vmimpl->stack_mem_begin);

                        // TODO: DO CLEAN
                        if (vmimpl->env->_created_destructable_instance_count == 0)
                            need_destruct_gc_destructor_list.push_back(vmimpl);
                    }
                }

                if (_gc_stopping_world_gc)
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            if (!vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                                vmimpl->wakeup();

            } while (0);


            for (auto* destruct_vm : need_destruct_gc_destructor_list)
                delete destruct_vm;

            // All jobs done.
            _gc_stopping_world_gc = false;
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

    } // END NAME SPACE gc

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