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
        wo_vm vmm = wo_create_vm();
        bool compile_successful_flag = wo_load_file(vmm, argv[1]);

        if (compile_successful_flag)
        {
            wo_value    return_state = nullptr;

            const char* out_binary_path = nullptr;
            bool        out_binary_file_ok = false;

            for (int i = 0; i < argc - 1; ++i)
            {
                if (strcmp(argv[i], "-o") == 0)
                    out_binary_path = argv[i + 1];
            }
            if (out_binary_path == nullptr)
                return_state = wo_bootup(vmm, WO_TRUE);
            else
            {
                if (FILE* out_binary_file = fopen(out_binary_path, "wb"))
                {
                    size_t binary_len = 0;
                    void* binary_buf = wo_dump_binary(vmm, true, &binary_len);

                    if (fwrite(binary_buf, sizeof(char), binary_len, out_binary_file) == binary_len)
                        out_binary_file_ok = true;

                    fclose(out_binary_file);
                    wo_free_binary(binary_buf);
                }
            }

            if (return_state != nullptr)
            {
                if (wo_valuetype(return_state) == WO_INTEGER_TYPE)
                    ret = (int)wo_cast_int(return_state);
                else
                    ret = 0;
            }
            else if (out_binary_path != nullptr)
            {
                if (out_binary_file_ok)
                    ret = 0;
                else
                    ret = errno;
            }
            else
                ret = -1;
        }
        else
        {
            std::cerr << wo_get_compile_error(vmm, WO_DEFAULT) << std::endl;
            ret = -2;
        }

        wo_close_vm(vmm);
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
