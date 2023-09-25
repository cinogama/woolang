#include "wo_vm.hpp"
#include "wo_memory.hpp"

#include <thread>
#include <chrono>
#include <list>

// PARALLEL-GC SUPPORTED

#define WO_GC_FORCE_STOP_WORLD false
#define WO_GC_FORCE_FULL_GC false

namespace wo
{
    // A very simply GC system, just stop the vm, then collect inform
    namespace gc
    {
        uint16_t                    _gc_round_count = 0;
        constexpr uint16_t          _gc_work_thread_count = 4;
        constexpr uint16_t          _gc_max_count_to_move_young_to_old = 25;

        std::atomic_bool            _gc_stop_flag = false;
        std::thread                 _gc_scheduler_thread;
        std::condition_variable     _gc_work_cv;
        std::mutex                  _gc_work_mx;

        std::atomic_flag            _gc_immediately = {};

        uint32_t                    _gc_immediately_edge = 250000;
        uint32_t                    _gc_stop_the_world_edge = _gc_immediately_edge * 20;

        std::atomic_size_t _gc_scan_vm_index;
        size_t _gc_scan_vm_count;
        std::atomic<vmbase**> _gc_vm_list;

        std::atomic_bool _gc_is_marking = false;
        std::atomic_bool _gc_is_recycling = false;

        bool _gc_stopping_world_gc = WO_GC_FORCE_STOP_WORLD;
        bool _gc_advise_to_full_gc = WO_GC_FORCE_FULL_GC;

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

        atomic_list<gcbase::memo_unit> m_memo_mark_gray_list;
        std::list<std::pair<gcbase*, gcbase::unit_attrib*>> _gc_gray_unit_lists[_gc_work_thread_count];
        std::unordered_map<vmbase*, std::list<std::pair<gcbase*, gcbase::unit_attrib*>>> _gc_vm_gray_unit_lists;

        void gc_mark_unit_as_gray(std::list<std::pair<gcbase*, gcbase::unit_attrib*>>* worklist, gcbase* unitvalue, gcbase::unit_attrib* attr)
        {
            if (attr->m_marked == (uint8_t)gcbase::gcmarkcolor::no_mark)
            {
                attr->m_marked = (uint8_t)gcbase::gcmarkcolor::self_mark;
                worklist->push_back(std::make_pair(unitvalue, attr));
            }
        }

        void gc_mark_unit_as_black(std::list<std::pair<gcbase*, gcbase::unit_attrib*>>* worklist, gcbase* unit, gcbase::unit_attrib* unitattr)
        {
            if (unitattr->m_marked == (uint8_t)gcbase::gcmarkcolor::full_mark)
                return;

            wo_assert(unitattr->m_marked == (uint8_t)gcbase::gcmarkcolor::self_mark);
            unitattr->m_marked = (uint8_t)gcbase::gcmarkcolor::full_mark;

            gcbase::unit_attrib* attr;

            wo::gcbase::gc_read_guard g1(unit);
            if (array_t* wo_arr = dynamic_cast<array_t*>(unit))
            {
                for (auto& val : *wo_arr)
                {
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier(&attr))
                        gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
                }
            }
            else if (dict_t* wo_map = dynamic_cast<dict_t*>(unit))
            {
                for (auto& [key, val] : *wo_map)
                {
                    if (gcbase* gcunit_addr = key.get_gcunit_with_barrier(&attr))
                        gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
                    if (gcbase* gcunit_addr = val.get_gcunit_with_barrier(&attr))
                        gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
                }
            }
            else if (gchandle_t* wo_gchandle = dynamic_cast<gchandle_t*>(unit))
            {
                if (gcbase* gcunit_addr = wo_gchandle->m_holding_value.get_gcunit_with_barrier(&attr))
                    gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
            }
            else if (closure_t* wo_closure = dynamic_cast<closure_t*>(unit))
            {
                for (uint16_t i = 0; i < wo_closure->m_closure_args_count; ++i)
                    if (gcbase* gcunit_addr = wo_closure->m_closure_args[i].get_gcunit_with_barrier(&attr))
                        gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
            }
            else if (struct_t* wo_struct = dynamic_cast<struct_t*>(unit))
            {
                for (uint16_t i = 0; i < wo_struct->m_count; ++i)
                    if (gcbase* gcunit_addr = wo_struct->m_values[i].get_gcunit_with_barrier(&attr))
                        gc_mark_unit_as_gray(worklist, gcunit_addr, attr);
            }
        }

        void gc_mark_all_gray_unit(std::list<std::pair<gcbase*, gcbase::unit_attrib*>>* worklist)
        {
            std::list<std::pair<gcbase*, gcbase::unit_attrib*>> graylist;
            while (!worklist->empty())
            {
                graylist.clear();
                graylist.swap(*worklist);
                for (auto [markingunit, attrib] : graylist)
                {
                    gc_mark_unit_as_black(worklist, markingunit, attrib);
                }
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

                                        gcbase::unit_attrib* attr;
                                        gcbase* gcunit_address = global_val->get_gcunit_with_barrier(&attr);
                                        if (gcunit_address)
                                            gc_mark_unit_as_gray(&_gc_gray_unit_lists[worker_id], gcunit_address, attr);
                                    }
                                }
                            }
                        }
                        else
                        {
                            mark_vm(marking_vm, worker_id);

                            if (_gc_stopping_world_gc == false)
                                if (!marking_vm->clear_interrupt(vmbase::GC_HANGUP_INTERRUPT))
                                    marking_vm->wakeup();
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

                    gc_mark_all_gray_unit(&_gc_gray_unit_lists[worker_id]);

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

        bool _gc_work_list(bool stopworld, bool fullgc)
        {
#ifdef WO_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"wo_gc_main");
#endif
            // Pick all gcunit before 1st mark begin.
            // It means all unit alloced when marking will be skiped to free.
            std::vector<vmbase*> gc_marking_vmlist, self_marking_vmlist, time_out_vmlist;

            // 0. get current vm list, set stop world flag to TRUE:
            do
            {
                std::shared_lock sg1(vmbase::_alive_vm_list_mx); // Lock alive vm list, block new vm create.
                _gc_round_count++;

                // Ignore old memo, they are useless.
                auto* old_mem_units = m_memo_mark_gray_list.pick_all();
                while (old_mem_units)
                {
                    auto* cur_unit = old_mem_units;
                    old_mem_units = old_mem_units->last;

                    delete cur_unit;
                }
                _gc_is_marking = true;

                // 0. Prepare vm gray unit list
                if (!stopworld)
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
                            case vmbase::interrupt_wait_result::LEAVED:
                                if (vmimpl->interrupt(vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                                {
                                    // NOTE: In fact, there is a very small probability that the 
                                    //      vm just completes self-marking within a subtle time interval.
                                    //      So we need recheck for GC_INTERRUPT
                                    if (vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                                        // Current VM not receive GC_INTERRUPT, and we already mark GC_HANGUP_INTERRUPT
                                        // the vm will be mark by gc-worker-thread.
                                        break;

                                    // NOTE: Oh! the small probability event happened! the vm has been
                                    //       self marked. We need clear GC_INTERRUPT & GC_HANGUP_INTERRUPT
                                    //       and wake up the vm;
                                    if (!vmimpl->clear_interrupt(vmbase::GC_HANGUP_INTERRUPT))
                                        // NOTE: GC_HANGUP_INTERRUPT has been received by vm, we need wake it up.
                                        vmimpl->wakeup();
                                }
                            case vmbase::interrupt_wait_result::TIMEOUT:
                            case vmbase::interrupt_wait_result::ACCEPT:
                                // Current vm is self marking...
                                self_marking_vmlist.push_back(vmimpl);
                                continue;
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
                        {
                            // NOTE: See gc_checkpoint, we have very small probability when last round
                            // stw GC, an vm still in gc_checkpoint and mark GC_HANGUP_INTERRUPT it self,
                            // we need mark until success.
                            while (!vmimpl->interrupt(vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT));
                        }
                    }

                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                            vmimpl->wait_interrupt(vmbase::GC_HANGUP_INTERRUPT);

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
                if (!stopworld)
                {
                    // 5. Wait until all self-marking vm work finished
                    for (auto* vmimpl : self_marking_vmlist)
                    {
                        auto self_mark_gc_state = vmimpl->wait_interrupt(vmbase::GC_INTERRUPT);
                        wo_assert(vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL);
                        wo_assert(self_mark_gc_state != vmbase::interrupt_wait_result::LEAVED);
                        if (self_mark_gc_state == vmbase::interrupt_wait_result::TIMEOUT)
                        {
                            // Current vm is still structed, let it hangup and mark it here.
                            // NOTE: If GC_HANGUP_INTERRUPT interrupt successfully, it means:
                            //      1. Current vm still not receive GC_INTERRUPT
                            //      2. Current vm already receive GC_INTERRUPT, but don't interrupt GC_HANGUP_INTERRUPT
                            //          in this moment, vm will skip self mark and continue handle GC_HANGUP_INTERRUPT 
                            //          to hangup
                            //      3. Current vm already receive GC_INTERRUPT, and it has already finish mark itself
                            //          in this moment, we will not able to clear GC_INTERRUPT
                            //      In case 1, 2: we will clear GC_INTERRUPT successfully, and we will mark the vm here
                            //      But in case 3, we will not able to clear GC_INTERRUPT, vm has already mark it self.
                            //          we will skip mark.
                            if (vmimpl->interrupt(vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                            {
                                // Check if this vm still have GC_INTERRUPT, if not, it means the vm has been 
                                // self-marked.
                                if (vmimpl->clear_interrupt(vmbase::vm_interrupt_type::GC_INTERRUPT))
                                {
                                    mark_vm(vmimpl, SIZE_MAX);
                                }

                                if (!vmimpl->clear_interrupt(vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                                    vmimpl->wakeup();
                            }
                        }
                        // Wait until specify vm self-marking end.

                        vmbase::interrupt_wait_result wait_result;
                        do
                        {
                            wait_result = vmimpl->wait_interrupt(vmbase::GC_HANGUP_INTERRUPT);
                            wo_assert(wait_result != vmbase::interrupt_wait_result::LEAVED);
                        } while (wait_result == vmbase::interrupt_wait_result::TIMEOUT);
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

            // 4. OK, Continue mark gray to black
            _gc_mark_thread_groups::instancce().launch_round_of_mark();

            // Marking finished.
            // NOTE: It is safe to do this here, because if the mark has ended, 
            //      if there is a unit trying to enter the memory set at the same time
            //      there are only two possibilities:
            //      1) This unit is a new unit generated during the GC process. In this case, 
            //          this unit will be regarded as marked and will not be recycled.
            //      2) This unit is being removed from a fullmark object. In this case, 
            //          it means that this object is actually attached to the fullmark object 
            //          during the GC process. Since this unit is still in the nomark stage, 
            //          it means that this unit is either generated during the gc process like case 1,
            //          or it is detached from other objects and is not marked: for this case, 
            //          this unit should have entered the memory set, it's safe to skip.
            _gc_is_marking = false;

            // 4.1 Collect gray units in memo set.
            std::list<std::pair<gcbase*, gcbase::unit_attrib*>> mem_gray_list;
            auto* memo_units = m_memo_mark_gray_list.pick_all();
            while (memo_units)
            {
                auto* cur_unit = memo_units;
                memo_units = memo_units->last;

                gc_mark_unit_as_gray(&mem_gray_list, cur_unit->gcunit, cur_unit->gcunit_attr);
                delete cur_unit;
            }
            gc_mark_all_gray_unit(&mem_gray_list);

            _gc_is_recycling = true;

            // 5. OK, All unit has been marked. reduce gcunits
            gcbase::unit_attrib alloc_dur_current_gc_attrib_mask = {}, alloc_dur_current_gc_attrib = {};
            alloc_dur_current_gc_attrib_mask.m_gc_age = (uint8_t)0x0F;
            alloc_dur_current_gc_attrib_mask.m_alloc_mask = (uint8_t)0x01;
            alloc_dur_current_gc_attrib.m_gc_age = (uint8_t)0x0F;
            alloc_dur_current_gc_attrib_mask.m_alloc_mask = (uint8_t)_gc_round_count & (uint8_t)0x01;

            size_t page_count, page_size, alive_unit = 0;
            char* pages = (char*)womem_enum_pages(&page_count, &page_size);
            for (size_t pageidx = 0; pageidx < page_count; ++pageidx)
            {
                size_t unit_count, unit_size;
                char* units = (char*)womem_get_unit_buffer(pages + pageidx * page_size, &unit_count, &unit_size);

                if (units == nullptr)
                    continue;

                for (size_t unitidx = 0; unitidx < unit_count; ++unitidx)
                {
                    gcbase::unit_attrib* attr;
                    void* unit = womem_get_unit_ptr_attribute(units + unitidx * unit_size,
                        std::launder(reinterpret_cast<womem_attrib_t**>(&attr)));
                    if (unit != nullptr)
                    {
                        if (attr->m_marked == (uint8_t)gcbase::gcmarkcolor::no_mark &&
                            (attr->m_gc_age != 0 || fullgc) &&
                            attr->m_nogc == 0 &&
                            (attr->m_attr & alloc_dur_current_gc_attrib_mask.m_attr)
                            != alloc_dur_current_gc_attrib_mask.m_attr)
                        {
                            // This unit didn't been mark. and not alloced during this round.
                            std::launder(reinterpret_cast<gcbase*>(unit))->~gcbase();
                            free64(unit);
                        }
                        else
                        {
                            ++alive_unit;
                            wo_assert(attr->m_marked != (uint8_t)gcbase::gcmarkcolor::self_mark);
                            attr->m_marked = (uint8_t)gcbase::gcmarkcolor::no_mark;

                            if (attr->m_gc_age >= 0)
                                --attr->m_gc_age;
                        }
                    }
                }
            }
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

                if (stopworld)
                {
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                    {
                        if (vmimpl->virtual_machine_type == vmbase::vm_type::NORMAL)
                        {
                            if (!vmimpl->clear_interrupt(vmbase::GC_HANGUP_INTERRUPT))
                                vmimpl->wakeup();
                        }
                    }
                }

            } while (0);


            for (auto* destruct_vm : need_destruct_gc_destructor_list)
                delete destruct_vm;

            womem_tidy_pages(_gc_advise_to_full_gc ? 1 : 0);

            _gc_is_recycling = false;

            // All jobs done.

            return alive_unit != 0;
        }

        void _gc_main_thread()
        {
            do
            {
                if (_gc_round_count % 1000 == 0)
                    _gc_advise_to_full_gc = true;

                _gc_work_list(_gc_stopping_world_gc, _gc_advise_to_full_gc);

                _gc_stopping_world_gc = WO_GC_FORCE_STOP_WORLD;
                _gc_advise_to_full_gc = WO_GC_FORCE_FULL_GC;

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

            while (_gc_work_list(true, true))
            {
                using namespace std;
                std::this_thread::sleep_for(10ms);
            }
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

                    gcbase::unit_attrib* attr;
                    gcbase* gcunit_address = self_reg_walker->get_gcunit_with_barrier(&attr);
                    if (gcunit_address)
                        gc_mark_unit_as_gray(worklist, gcunit_address, attr);

                }

                // walk thorgh stack.
                for (auto* stack_walker = marking_vm->stack_mem_begin/*env->stack_begin*/;
                    marking_vm->sp < stack_walker;
                    stack_walker--)
                {
                    auto stack_val = stack_walker;

                    gcbase::unit_attrib* attr;
                    gcbase* gcunit_address = stack_val->get_gcunit_with_barrier(&attr);
                    if (gcunit_address)
                        gc_mark_unit_as_gray(worklist, gcunit_address, attr);
                }
            }
        }

        void alloc_failed_retry()
        {
            wo_gc_immediately(true);

            auto* curvm = (wo_vm)wo::vmbase::_this_thread_vm;
            bool need_re_entry = true;
            if (curvm)
                need_re_entry = wo_leave_gcguard(curvm);

            using namespace std;
            std::this_thread::sleep_for(0.1s);

            if (curvm && need_re_entry)
                wo_enter_gcguard(curvm);
        }
    } // END NAME SPACE gc

    void gcbase::write_barrier(const value* val)
    {
        gcbase::unit_attrib* attr;
        if (auto* mem = val->get_gcunit_with_barrier(&attr))
        {
            if (attr->m_marked == (uint8_t)gcbase::gcmarkcolor::no_mark)
            {
                gc::m_memo_mark_gray_list.add_one(
                    new gcbase::memo_unit{ mem, attr });
            }
        }
    }

    bool gc_handle_base_t::close()
    {
        void* holding_handle = m_holding_handle.exchange(nullptr);
        if (holding_handle != nullptr)
        {
            wo_assert(m_destructor != nullptr && m_gc_vm != nullptr);

            m_destructor(holding_handle);
            std::launder(reinterpret_cast<wo::vmbase*>(m_gc_vm))
                ->dec_destructable_instance_count();
            return true;
        }
        return false;
    }

    void* alloc64(size_t memsz, womem_attrib_t attrib)
    {
        bool warn = true;
        for (;;)
        {
            gcbase::unit_attrib attr = {};
            attr.m_alloc_mask = (uint8_t)gc::_gc_round_count & (uint8_t)0b01;
            if (auto* p = womem_alloc(memsz, attrib | attr.m_attr))
                return p;

            // Memory is not enough.
            if (warn)
            {
                warn = false;
                wo_warning("Out of memory, trying GC for extra memory");
            }
            wo_gc_immediately(true);

            auto* curvm = (wo_vm)wo::vmbase::_this_thread_vm;
            bool need_re_entry = true;
            if (curvm)
                need_re_entry = wo_leave_gcguard(curvm);

            using namespace std;
            std::this_thread::sleep_for(0.1s);

            if (curvm && need_re_entry)
                wo_enter_gcguard(curvm);
        }
    }
    void free64(void* ptr)
    {
        womem_free(ptr);
    }
}

void wo_gc_immediately(wo_bool_t fullgc)
{
    std::lock_guard g1(wo::gc::_gc_work_mx);
    wo::gc::_gc_advise_to_full_gc = true;
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
