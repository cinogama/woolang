#include "wo.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    int ret = 0;

    if (argc >= 2)
    {
        wo_CompileErrors* compile_error;
        woort_codeenv* cenv = wo_load_file(argv[1], &compile_error);
        if (cenv != NULL)
        {
            woort_vm* vmm = woort_vm_create();            
            if (vmm == NULL)
            {
                std::cerr << "Failed to create VM instance: out of memory." << std::endl;
                ret = -3;
            }
            else
            {
                (void)woort_vm_swap(vmm);

                woort_value v;
                if (!woort_push_reserve(1, &v))
                {
                    std::cerr << "Failed to run: cannot reserve stack." << std::endl;
                    ret = -3;

                    goto label_run_failed;
                }

                if (woort_bootup_codeenv(v, cenv) == WOORT_VM_CALL_STATUS_NORMAL)
                    ret = (int)woort_int(v);
                else
                    ret = -1;

            label_run_failed:
                (void)woort_vm_swap(NULL);
            }
            woort_codeenv_drop(cenv);
            woort_vm_close(vmm);
        }
        else
        {
            if (compile_error != nullptr)
            {
                std::cerr << wo_get_compile_error(compile_error, WO_DEFAULT)
                          << std::endl;
                wo_compile_errors_free(compile_error);
            }
            ret = -2;
        }
    }
    else
    {
        std::cout << "Woolang (c) 2021 Cinogama project." << std::endl;
        std::cout << std::endl;
        std::cout << "Version: " << wo_version() << std::endl;
        std::cout << "Commit: " << wo_commit_sha() << std::endl;
        std::cout << "Date: " << wo_compile_date() << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: " << std::endl;
        std::cout << "    woolang <path> [options...]" << std::endl;

    }
    wo_finish(nullptr, nullptr);
    return ret;
}
