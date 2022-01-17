#pragma once

// rs_roroutine_mgr is based on virtual machine's runtime
// rs_roroutine_thread_mgr will based on Fiber/UContext
//

#include "rs_roroutine_thread.hpp"
#include "rs_vm.hpp"

namespace rs
{
    class vmthread
    {
        fthread m_fthread;

        vmbase* m_virtualmachine = nullptr;
        size_t m_argc = 0;

        enum vm_invoke_method_type
        {
            VM_METHOD,
            NATIVE_METHOD,
        };

        vm_invoke_method_type m_invoke_method;

        union
        {
            rs_int_t m_vm_funcaddr;
            rs_handle_t m_native_funcaddr;
        };

        static void _vmthread_invoker(vmthread* _this)
        {
            if (_this->m_virtualmachine)
            {

            }
        }
    public:
        vmthread()
            :m_fthread(_vmthread_invoker, this)
        {

        }
    };
}
