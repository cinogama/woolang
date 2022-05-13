#define _CRT_SECURE_NO_WARNINGS
#include "rs_vm.hpp"
#include "rs_memory.hpp"

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

namespace rs
{
    // A very simply GC system, just stop the vm, then collect inform
// #define RS_GC_DEBUG
    namespace gc
    {
        uint16_t                    _gc_round_count = 0;
        constexpr uint16_t          _gc_work_thread_count = 4;
        constexpr uint16_t          _gc_max_count_to_move_young_to_old = 5;
        std::condition_variable     _gc_cv;

        uint32_t                    _gc_immediately_edge = 50000;
        uint32_t                    _gc_stop_the_world_edge = _gc_immediately_edge * 5;

        // If vmbase::_alive_vm_count_for_gc_vm_destruct > _gc_immediately_vm_count_edge,
        // gc work will start immediately, either.
        uint32_t                    _gc_immediately_vm_count_edge = 200;

        volatile bool               _gc_fullgc_stopping_the_world = false;
        std::atomic_flag            _gc_immediately_manually_flag = {};

        std::atomic_flag            _gc_stop_flag = {};
        std::thread                 _gc_scheduler_thread;

        void deep_in_to_mark_unit(gcbase* unit)
        {
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::full_mark)
                return;

            unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::full_mark);

            rs::gcbase::gc_read_guard g1(unit);

            if (array_t* rs_arr = dynamic_cast<array_t*>(unit))
            {
                for (auto& val : *rs_arr)
                {
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        deep_in_to_mark_unit(gcunit_addr);
                }
            }
            else if (mapping_t* rs_map = dynamic_cast<mapping_t*>(unit))
            {
                for (auto& [key, val] : *rs_map)
                {
                    if (gcbase* gcunit_addr = key.get_gcunit_with_barrier())
                        deep_in_to_mark_unit(gcunit_addr);
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        deep_in_to_mark_unit(gcunit_addr);
                }
            }
            else if (gchandle_t* rs_gchandle = dynamic_cast<gchandle_t*>(unit))
            {
                if (gcbase* gcunit_addr = rs_gchandle->holding_value.get_gcunit_with_barrier())
                    deep_in_to_mark_unit(gcunit_addr);
            }

        } // void deep_in_to_mark_unit(gcbase * unit)

        class gc_mark_thread
        {
            std::mutex _gc_mark_fence_mx;
            std::condition_variable _gc_mark_fence_cv;
            volatile bool _gc_start_working_flag = false;
            volatile bool _gc_working_end_flag = false;
            volatile bool _gc_ready = false;

            std::thread _gc_mark_work_thread;
            std::vector<rs::vmbase*> _gc_working_vm;

            std::atomic_flag _gcwork_stop_flag = {};

            static void _gc_work_func(gc_mark_thread* gmt)
            {
                gmt->_gcwork_stop_flag.test_and_set();

#ifdef RS_PLATRORM_OS_WINDOWS
                SetThreadDescription(GetCurrentThread(), L"rs_gc_worker_thread");
#endif
                while (true)
                {
                    std::unique_lock ug1(gmt->_gc_mark_fence_mx);
                    gmt->_gc_mark_fence_cv.wait(ug1, [gmt]() {gmt->_gc_ready = true; return gmt->_gc_start_working_flag; });
                    gmt->_gc_start_working_flag = false;

                    if (!gmt->_gcwork_stop_flag.test_and_set())
                        break;

                    for (auto* vmimpl : gmt->_gc_working_vm)
                    {
                        // do mark

                        // ...
                        // Walk thorw CONST & GLOBAL, REG, STACK_BEGIN TO SP;

                        // if vm has env:

                        if (auto env = vmimpl->env;
                            vmimpl->virtual_machine_type != vmbase::vm_type::GC_DESTRUCTOR
                            && env)
                        {
                            // walk thorgh global.
                            for (int cgr_index = 0;
                                cgr_index < env->constant_and_global_value_takeplace_count;
                                cgr_index++)
                            {
                                auto global_val = env->constant_global_reg_rtstack + cgr_index;

                                gcbase* gcunit_address = global_val->get_gcunit_with_barrier();
                                if (gcunit_address)
                                {
                                    // mark it

                                    // TODO: HERE USING STOPPED-GC, USING 3-COLOR IN FUTURE.
                                    deep_in_to_mark_unit(gcunit_address);
                                }
                            }

                            for (int reg_index = 0;
                                reg_index < env->real_register_count;
                                reg_index++)
                            {
                                auto self_reg_walker = vmimpl->register_mem_begin + reg_index;

                                gcbase* gcunit_address = self_reg_walker->get_gcunit_with_barrier();
                                if (gcunit_address)
                                {
                                    // mark it

                                    // TODO: HERE USING STOPPED-GC, USING 3-COLOR IN FUTURE.
                                    deep_in_to_mark_unit(gcunit_address);
                                }
                            }

                            // walk thorgh stack.
                            for (auto* stack_walker = vmimpl->stack_mem_begin/*env->stack_begin*/;
                                vmimpl->sp < stack_walker;
                                stack_walker--)
                            {
                                auto stack_val = stack_walker;

                                gcbase* gcunit_address = stack_val->get_gcunit_with_barrier();
                                if (gcunit_address)
                                {
                                    // mark it
                                    deep_in_to_mark_unit(gcunit_address);
                                }
                            }
                        }

                    }

                    gmt->_gc_working_vm.clear();

                    gmt->_gc_working_end_flag = true;
                    ug1.unlock();

                    gmt->_gc_mark_fence_cv.notify_one();
                }

            }
        public:

            gc_mark_thread()
                :_gc_mark_work_thread(_gc_work_func, this)
            {
                while (!_gc_ready)
                    std::this_thread::yield();
            }

            ~gc_mark_thread()
            {

            }

            inline void append(rs::vmbase* vmimpl)
            {
                _gc_working_vm.push_back(vmimpl);
            }

            inline void start()
            {
                do {
                    std::lock_guard g1(_gc_mark_fence_mx);
                    _gc_start_working_flag = true;

                } while (0);
                _gc_mark_fence_cv.notify_one();
            }

            inline void wait()
            {
                std::unique_lock ug1(_gc_mark_fence_mx);
                _gc_mark_fence_cv.wait(ug1, [this]() {return _gc_working_end_flag; });

                _gc_working_end_flag = false;
            }

            inline void stop()
            {
                _gcwork_stop_flag.clear();
                start();
                _gc_mark_work_thread.join();
            }
        };

        void mark_nogc_child(gcbase* picked_list)
        {
            while (picked_list)
            {
                if (picked_list->gc_type == gcbase::gctype::no_gc
                    && gcbase::gcmarkcolor::no_mark == picked_list->gc_marked(_gc_round_count))
                    deep_in_to_mark_unit(picked_list);

                picked_list = picked_list->last;
            }
        }

        void check_and_move_edge_to_edge(gcbase* picked_list,
            rs::atomic_list<rs::gcbase>* origin_list,
            rs::atomic_list<rs::gcbase>* aim_edge,
            rs::gcbase::gctype aim_gc_type,
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
                    if (!origin_list || ((picked_list->gc_type == gcbase::gctype::eden
                        || picked_list->gc_mark_alive_count > max_count) && aim_edge))
                    {
                        gcbase::gc_write_guard gcwg1(picked_list);

                        // over count, move it to old_edge.
                        aim_edge->add_one(picked_list);
                        picked_list->gc_type = aim_gc_type;
                    }
                    else
                    {
                        gcbase::gc_write_guard gcwg1(picked_list);

                        // add it back
                        origin_list->add_one(picked_list);
                    }
                }

                picked_list = last;
            }
        }

        void gc_work_thread()
        {
#ifdef RS_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"rs_gc_scheduler_thread");
#endif
            using namespace std;

            std::mutex          _gc_mx;
            gc_mark_thread      _gc_work_threads[_gc_work_thread_count];

#ifdef RS_GC_DEBUG
            std::chrono::system_clock _gc_debug_system_clock;
#endif

            do
            {
                if (!_gc_stop_flag.test_and_set())
                    break;  // BREAK TO STOP GC WORK THREAD

                _gc_round_count++;

                int vm_distribute_index = 0;

                do {
                    std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                    if (vmbase::_alive_vm_list.empty())
                        break;

                    if (!_gc_fullgc_stopping_the_world)
                    {
                        for (auto* vmimpl : vmbase::_alive_vm_list)
                            if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                vmimpl->interrupt(vmbase::GC_INTERRUPT);

                        for (auto* vmimpl : vmbase::_alive_vm_list)
                            if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                vmimpl->wait_interrupt(vmbase::GC_INTERRUPT);
                    }

                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        _gc_work_threads[(vm_distribute_index++) % _gc_work_thread_count].append(vmimpl);


#ifdef RS_GC_DEBUG
                    rs_stdout << "===== GC WORK BEGIN =====" << std::endl;
                    auto _gcdebug_start_time = _gc_debug_system_clock.now();
#endif
                    for (auto& gc_work_thread : _gc_work_threads)
                        gc_work_thread.start();

                    for (auto& gc_work_thread : _gc_work_threads)
                        gc_work_thread.wait();

                    if (!_gc_fullgc_stopping_the_world)
                    {
                        for (auto* vmimpl : vmbase::_alive_vm_list)
                            if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                if (!vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                                    vmimpl->wakeup();
                    }

#ifdef RS_GC_DEBUG   
                    auto _gcdebug_end_time = _gc_debug_system_clock.now();
                    rs_stdout << "GC Time cost: " << (_gcdebug_end_time - _gcdebug_start_time).count() / 10000000.0f << std::endl;
                    rs_stdout << "===== GC WORK END =====" << std::endl;
#endif

                } while (0);

                // just full gc:
                auto* eden_list = gcbase::eden_age_gcunit_list.pick_all();
                auto* young_list = gcbase::young_age_gcunit_list.pick_all();
                auto* old_list = gcbase::old_age_gcunit_list.pick_all();

                // Mark all no_gc_object
                mark_nogc_child(eden_list);
                mark_nogc_child(young_list);
                mark_nogc_child(old_list);

                check_and_move_edge_to_edge(old_list, &gcbase::old_age_gcunit_list, nullptr, gcbase::gctype::old, UINT16_MAX);
                check_and_move_edge_to_edge(young_list, &gcbase::young_age_gcunit_list, &gcbase::old_age_gcunit_list, gcbase::gctype::old, _gc_max_count_to_move_young_to_old);
                // Move all eden to young
                check_and_move_edge_to_edge(eden_list, nullptr, &gcbase::young_age_gcunit_list, gcbase::gctype::young, 0);

                // if full gc 
                // delete unref elem, move others back to list

                // Manage young,
                // delete unref elem, move others back to list,
                // if scan over spcfy count, move elem to old

                // do gc destruct here..


                // check all destructor vm, if vm's env has only one runner, and all instance
                // has been closed, destroy it..
                std::list<vmbase*> need_destruct_gc_destructor_list;

                do
                {
                    std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::GC_DESTRUCTOR)
                        {
                            if (vmimpl->env->_running_on_vm_count == 1)
                            {
                                // assure vm's stack if empty
                                rs_assert(vmimpl->sp == vmimpl->bp && vmimpl->bp == vmimpl->stack_mem_begin);

                                // TODO: DO CLEAN

                                if (vmimpl->env->_created_destructable_instance_count == 0)
                                {
                                    need_destruct_gc_destructor_list.push_back(vmimpl);
                                }

                            }
                        }
                    }
                } while (0);

                for (auto* destruct_vm : need_destruct_gc_destructor_list)
                {
                    delete destruct_vm;
                }


                // -----Run MINOR-GC once per 10 sec, Run FULL-GC once per 5 MINOR-GC
                // -----Or you can awake gc-work manually.
                // Here to modify gc setting..

                using namespace std;

                if (_gc_immediately_manually_flag.test_and_set())
                {
                    if (gcbase::gc_new_count > _gc_stop_the_world_edge)
                    {
                        // Stop world immediately, then reboot gc...
                        if (!_gc_fullgc_stopping_the_world)
                        {
                            std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                            for (auto* vmimpl : vmbase::_alive_vm_list)
                                if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                    vmimpl->interrupt(vmbase::vm_interrupt_type::GC_INTERRUPT);

                            for (auto* vmimpl : vmbase::_alive_vm_list)
                                if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                    vmimpl->wait_interrupt(vmbase::vm_interrupt_type::GC_INTERRUPT);
                        }

                        gcbase::gc_new_count -= _gc_stop_the_world_edge;

                        _gc_fullgc_stopping_the_world = true;

                        continue;
                    }

                    if (_gc_fullgc_stopping_the_world)
                    {
                        _gc_fullgc_stopping_the_world = false;

                        std::shared_lock sg1(vmbase::_alive_vm_list_mx);

                        for (auto* vmimpl : vmbase::_alive_vm_list)
                            if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                                if (!vmimpl->clear_interrupt(vmbase::vm_interrupt_type::GC_INTERRUPT))
                                    vmimpl->wakeup();
                        continue;
                    }

                    if (gcbase::gc_new_count < _gc_immediately_edge
                        && vmbase::_alive_vm_count_for_gc_vm_destruct < _gc_immediately_vm_count_edge)
                    {
                        std::unique_lock ug1(_gc_mx);

                        for (int i = 0; i < 100; i++)
                        {
                            _gc_cv.wait_for(ug1, 0.1s);
                            if (gcbase::gc_new_count >= _gc_immediately_edge
                                || vmbase::_alive_vm_count_for_gc_vm_destruct >= _gc_immediately_vm_count_edge
                                || !_gc_immediately_manually_flag.test_and_set())
                                goto gc_immediately;
                        }
                    }
                    else
                    {
                    gc_immediately:
                        if (gcbase::gc_new_count >= _gc_immediately_edge)
                            gcbase::gc_new_count -= _gc_immediately_edge;
                        if (vmbase::_alive_vm_count_for_gc_vm_destruct >= _gc_immediately_vm_count_edge)
                            vmbase::_alive_vm_count_for_gc_vm_destruct -= _gc_immediately_vm_count_edge;
                    }
                }

            } while (true);

            for (auto& gcwork_thread : _gc_work_threads)
                gcwork_thread.stop();

        }

        void gc_start()
        {
            _gc_stop_flag.test_and_set();
            _gc_scheduler_thread = std::move(std::thread(gc_work_thread));
        }

        uint16_t gc_work_round_count()
        {
            return _gc_round_count;
        }

        void gc_begin(bool full_gc)
        {
            _gc_cv.notify_all();
        }

    }// namespace gc

    namespace gc
    {
        std::atomic_size_t _gc_scan_vm_index;
        volatile size_t _gc_scan_vm_count;
        vmbase** volatile _gc_vm_list;

        volatile bool _gc_is_marking = false;

        bool gc_is_marking()
        {
            return _gc_is_marking;
        }

        vmbase* _get_next_mark_vm(vmbase::vm_type* out_vm_type)
        {
            size_t id = _gc_scan_vm_index++;
            if (id < _gc_scan_vm_count)
            {
                *out_vm_type = _gc_vm_list[id]->virtual_machine_type;
                return _gc_vm_list[id];
            }

            return nullptr;
        }

        struct gray_gc_unit
        {
            gcbase* m_unit;
            gray_gc_unit* last;
        };
        atomic_list<gray_gc_unit> _gc_gray_unit_lists;

        void gc_mark_unit_as_gray(gcbase* unit)
        {
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::no_mark)
            {
                unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::self_mark);

                // TODO: Optmize here, donot use new.
                _gc_gray_unit_lists.add_one(new gray_gc_unit{ unit });
            }
        }

        void gc_mark_unit_as_black(gcbase* unit)
        {
            // TODO: Make 'gc_mark' atomicable
            if (unit->gc_marked(_gc_round_count) == gcbase::gcmarkcolor::full_mark)
                return;

            unit->gc_mark(_gc_round_count, gcbase::gcmarkcolor::full_mark);

            rs::gcbase::gc_read_guard g1(unit);

            if (array_t* rs_arr = dynamic_cast<array_t*>(unit))
            {
                for (auto& val : *rs_arr)
                {
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(gcunit_addr);
                }
            }
            else if (mapping_t* rs_map = dynamic_cast<mapping_t*>(unit))
            {
                for (auto& [key, val] : *rs_map)
                {
                    if (gcbase* gcunit_addr = key.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(gcunit_addr);
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier())
                        gc_mark_unit_as_gray(gcunit_addr);
                }
            }
            else if (gchandle_t* rs_gchandle = dynamic_cast<gchandle_t*>(unit))
            {
                if (gcbase* gcunit_addr = rs_gchandle->holding_value.get_gcunit_with_barrier())
                    gc_mark_unit_as_gray(gcunit_addr);
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
                    while (marking_vm = _get_next_mark_vm(&vm_type))
                    {
                        if (auto env = marking_vm->env)
                        {
                            if (vm_type == vmbase::vm_type::GC_DESTRUCTOR)
                            {
                                // Any code context only have one GC_DESTRUCTOR, here to mark global space.
                                for (int cgr_index = 0;
                                    cgr_index < env->constant_and_global_value_takeplace_count;
                                    cgr_index++)
                                {
                                    auto global_val = env->constant_global_reg_rtstack + cgr_index;

                                    gcbase* gcunit_address = global_val->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(gcunit_address);
                                }
                            }
                            else
                            {
                                rs_assert(vm_type == vmbase::vm_type::NORMAL);
                                // Mark stack, reg, 

                                // walk thorgh regs.
                                for (int reg_index = 0;
                                    reg_index < env->real_register_count;
                                    reg_index++)
                                {
                                    auto self_reg_walker = marking_vm->register_mem_begin + reg_index;

                                    gcbase* gcunit_address = self_reg_walker->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(gcunit_address);

                                }

                                // walk thorgh stack.
                                for (auto* stack_walker = marking_vm->stack_mem_begin/*env->stack_begin*/;
                                    marking_vm->sp < stack_walker;
                                    stack_walker--)
                                {
                                    auto stack_val = stack_walker;

                                    gcbase* gcunit_address = stack_val->get_gcunit_with_barrier();
                                    if (gcunit_address)
                                        gc_mark_unit_as_gray(gcunit_address);
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

        void _gc_work_list()
        {
            // 0. get current vm list, set stop world flag to TRUE:
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
            _gc_scan_vm_count = vmbase::_alive_vm_list.size();

            std::vector<vmbase*> vmlist(_gc_scan_vm_count);

            auto vm_index = vmbase::_alive_vm_list.begin();
            for (size_t i = 0; i < _gc_scan_vm_count; ++i)
                vmlist[i] = *(vm_index++);
            _gc_vm_list = vmlist.data();
            _gc_scan_vm_index = 0;

            // 3. Start GC Worker for first marking        
            _gc_mark_thread_groups::instancce().launch_round_of_mark();

            // just full gc:
            auto* eden_list = gcbase::eden_age_gcunit_list.pick_all();
            auto* young_list = gcbase::young_age_gcunit_list.pick_all();
            auto* old_list = gcbase::old_age_gcunit_list.pick_all();

            // Mark all no_gc_object
            mark_nogc_child(eden_list);
            mark_nogc_child(young_list);
            mark_nogc_child(old_list);

            // 4. OK, Continue mark gray to black
            gray_gc_unit* gray_units = nullptr;
            while (gray_units = _gc_gray_unit_lists.pick_all())
            {
                while (gray_units)
                {
                    auto* cur_unit = gray_units;
                    gray_units = gray_units->last;

                    gc_mark_unit_as_black(cur_unit->m_unit);

                    delete cur_unit;
                }
            }

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
                        rs_assert(vmimpl->sp == vmimpl->bp && vmimpl->bp == vmimpl->stack_mem_begin);

                        // TODO: DO CLEAN
                        if (vmimpl->env->_created_destructable_instance_count == 0)
                            need_destruct_gc_destructor_list.push_back(vmimpl);
                    }
                }
            } while (0);


            for (auto* destruct_vm : need_destruct_gc_destructor_list)
                delete destruct_vm;

            // All jobs done.
        }
    }

}


void rs_gc_immediately()
{
    rs::gc::_gc_immediately_manually_flag.clear();
}

void rs_gc_stop()
{
    rs::gc::_gc_stop_flag.clear();
    rs_gc_immediately();
    rs::gc::_gc_scheduler_thread.join();
}