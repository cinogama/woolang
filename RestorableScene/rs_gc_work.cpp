#include "rs_vm.hpp"

#include <thread>
#include <chrono>

namespace rs
{
    // A very simply GC system, just stop the vm, then collect inform

    namespace gc
    {
        uint16_t                    _gc_round_count = 0;
        constexpr uint16_t          _gc_work_thread_count = 4;
        std::condition_variable     _gc_cv;

        class gc_mark_thread
        {
            inline static std::mutex _gc_mark_fence_mx;
            inline static std::condition_variable _gc_mark_fence_cv;

            volatile bool _gc_start_working = false;
            volatile bool _gc_ready_flag = false;

            std::thread _gc_mark_work_thread;
            std::vector<rs::vmbase*> _gc_working_vm;

            static void _gc_work_func(gc_mark_thread* gmt)
            {
                while (true)
                {
                    std::unique_lock ug1(_gc_mark_fence_mx);
                    _gc_mark_fence_cv.wait(ug1, [gmt]() {gmt->_gc_ready_flag = true; return gmt->_gc_start_working; });

                    for (auto* vmimpl : gmt->_gc_working_vm)
                    {

                        rs_asure_warn(vmimpl->wait_interrupt(vmbase::GC_INTERRUPT),
                            "Virtual machine not accept GC_INTERRUPT. skip it.");

                        // do mark

                        // ...
                        // Walk thorw CONST & GLOBAL, REG, STACK_BEGIN TO SP;

                        // if vm has env:
                        if (vmimpl->cr)
                        {
                            auto& env = vmimpl->env;

                            // walk thorgh global.
                            for (int cgr_index = 0;
                                cgr_index < env.cgr_global_value_count;
                                cgr_index++)
                            {
                                auto& global_val = env.constant_global_reg_rtstack[cgr_index];
                                if ((uint8_t)global_val.type & (uint8_t)value::valuetype::need_gc)
                                {
                                    // mark it
                                    global_val.gcbase->gc_mark_color =  gcbase::gcmarkcolor::self_mark;
                                }
                            }

                            // walk thorgh stack.
                            for (auto* stack_walker = env.stack_begin;
                                vmimpl->sp < stack_walker;
                                stack_walker--)
                            {
                                if ((uint8_t)stack_walker->type & (uint8_t)value::valuetype::need_gc)
                                {
                                    // mark it
                                    stack_walker->gcbase->gc_mark_color = gcbase::gcmarkcolor::self_mark;
                                }
                            }
                        }
                        // over, if vm still not manage vmbase::GC_INTERRUPT clean it
                        // or wake up vm.
                        if (!vmimpl->clear_interrupt(vmbase::GC_INTERRUPT))
                            vmimpl->wakeup();

                    }
                    gmt->_gc_working_vm.clear();

                    gmt->_gc_start_working = false;
                    ug1.unlock();
                }

            }
        public:
            gc_mark_thread()
                :_gc_mark_work_thread(_gc_work_func, this)
            {
                while (!_gc_ready_flag)
                    std::this_thread::yield();
            }

            ~gc_mark_thread()
            {
                std::lock_guard g1(_gc_mark_fence_mx);

                while (_gc_start_working)
                    std::this_thread::yield();
            }

            inline void append(rs::vmbase* vmimpl)
            {
                rs_assert(!_gc_start_working);

                _gc_working_vm.push_back(vmimpl);
            }

            inline void start()
            {
                _gc_start_working = true;
                _gc_mark_fence_cv.notify_one();
            }

            inline void waitend()
            {
                using namespace std;

                while (_gc_start_working)
                    std::this_thread::sleep_for(0.1s);
            }
        };

        void gc_work_thread()
        {
            using namespace std;

            std::mutex          _gc_mx;
            gc_mark_thread      _gc_work_threads[_gc_work_thread_count];

            do
            {
                _gc_round_count++;

                int vm_distribute_index = 0;

                do {
                    std::shared_lock sg1(vmbase::_alive_vm_list_mx);
                    for (auto* vmimpl : vmbase::_alive_vm_list)
                        _gc_work_threads[(vm_distribute_index++) % _gc_work_thread_count].append(vmimpl);

                    for (gc_mark_thread& gc_work : _gc_work_threads)
                        gc_work.start();

                    for (gc_mark_thread& gc_work : _gc_work_threads)
                        gc_work.waitend();

                } while (0);

                // Mark end, do gc
                // Scan all eden, then scan all gray elem

                // if full gc 
                // delete unref elem, move others back to list

                // Manage young,
                // delete unref elem, move others back to list,
                // if scan over spcfy count, move elem to old

                // Move all eden to young
                
                // Run MINOR-GC once per 5 sec, Run FULL-GC once per 5 MINOR-GC
                // Or you can awake gc-work manually.

                using namespace std;

                std::unique_lock ug1(_gc_mx);
                _gc_cv.wait_for(ug1, 5s);

            } while (true);
        }

        void gc_start()
        {
            std::thread(gc_work_thread).detach();
        }

        uint16_t gc_work_round_count()
        {
            return _gc_round_count;
        }


    }// namespace gc
}