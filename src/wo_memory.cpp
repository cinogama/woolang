#include "wo_afx.hpp"

#define WOMEM_IMPL

#include "woomem.h"
#include "wo_memory.hpp"

namespace wo
{
    namespace mem
    {
        void alloc_failed_retry()
        {
            wo_gc_immediately(WO_TRUE);

            bool need_re_entry_gc_guard = true;

            auto* current_vm_instance = wo::vmbase::_this_thread_gc_guard_vm;
            wo::value* current_vm_stack_top = nullptr;

            if (current_vm_instance != nullptr)
            {
                // NOTE: We don't know the exactly state of current vm, so we need to 
                //       make sure all unit in current vm's stack and register are marked.
                current_vm_stack_top = current_vm_instance->sp;
                current_vm_instance->sp = current_vm_instance->stack_storage;

                need_re_entry_gc_guard =
                    wo_leave_gcguard(reinterpret_cast<wo_vm>(current_vm_instance));
            }

            using namespace std;
            std::this_thread::sleep_for(0.05s);

            if (current_vm_instance != nullptr)
            {
                if (need_re_entry_gc_guard)
                    wo_enter_gcguard(reinterpret_cast<wo_vm>(current_vm_instance));
                current_vm_instance->sp = current_vm_stack_top;
            }
        }

        void* gc_alloc(size_t memsz, int attrib)
        {
            bool warn = true;
            for (;;)
            {
                if (auto* p = woomem_alloc_attrib(
                    memsz,
                    WOOMEM_GC_UNIT_TYPE_NEED_SWEEP | attrib))
                {
                    return p;
                }

                // Memory is not enough.
                if (warn)
                {
                    warn = false;
                    std::string warning_info = "Out of memory, trying GC for extra memory.\n";
                    auto* cur_vm = wo::vmbase::_this_thread_gc_guard_vm;
                    if (cur_vm != nullptr)
                    {
                        std::stringstream dump_callstack_info;

                        cur_vm->dump_call_stack(32, false, dump_callstack_info);
                        warning_info += dump_callstack_info.str();
                    }
                    wo_warning(warning_info.c_str());
                }
                alloc_failed_retry();
            }
        }

        void _GC_marker_callback(
            woomem_UserContext /* useless */_useless, void* unit)
        {
            (void)_useless;

            GCunitBase* const gcunit = reinterpret_cast<GCunitBase*>(unit);
            gcunit->m_proxy->m_marker(gcunit);
        }
        void _GC_destroier_callback(
            woomem_UserContext /* useless */_useless, void* unit)
        {
            (void)_useless;

            GCunitBase* const gcunit = reinterpret_cast<GCunitBase*>(unit);
            gcunit->m_proxy->m_destroier(gcunit);
        }

        void _GC_start_gc_callback(void* /* useless */_useless)
        {
            (void)_useless;

            // TODO;
        }


        void init(void)
        {
            woomem_init(
                NULL,
                &_GC_marker_callback,
                &_GC_destroier_callback,
                &_GC_start_gc_callback,
                NULL);
        }
        void shutdown(void)
        {
            woomem_shutdown();
        }
    }
}
