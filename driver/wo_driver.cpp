#include "wo.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    int ret = 0;

//    wo_vm vm1 = wo_create_vm();
//    wo_load_source(vm1, "/vm1.wo", R"(
//        import woo::std; 
//        let mut s2 = "Helloworld2"; 
//        func super_depth_foo(f: ()=> string, s: string, n: int)=> void
//        {
//            if (n <= 0)
//                std::println(s, s2, f());
//            else
//                super_depth_foo(f, s, n - 1);
//        }
//        
//        func main()
//        {
//            let mut s = "Helloworld"; 
//            return \f: ()=> string = super_depth_foo(f, s, 10000000);;
//        }
//        return main();)");
//    wo_jit(vm1);
//    wo_value f = wo_run(vm1);
//
//    wo_vm vm2 = wo_create_vm();
//    wo_load_source(vm2, "/vm2.wo", R"(
//import woo::std; 
//
//let mut axab = "String From Vm2";
//func foo()
//{
//    return axab + "Yes~";
//}
//
//extern func main(f: (()=> string)=> void)
//{
//    let mut x = 128; 
//    f(foo); 
//    std::println("wtf", x, foo());
//})");
//    wo_jit(vm2);
//    wo_run(vm2);
//
//    wo_unref_value vm2main;
//    wo_extern_symb(&vm2main, vm2, "main");
//
//    wo_value s = wo_reserve_stack(vm2, 1, nullptr);
//    wo_set_val(s + 0, f);
//
//    wo_invoke_value(vm2, &vm2main, 1, nullptr, &s);
//
//    wo_pop_stack(vm2, 1);
//    wo_close_vm(vm1);
//    wo_close_vm(vm2);
//#if 0
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
            {
                wo_jit(vmm);
                return_state = wo_run(vmm);
            }
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
        std::cout << "Woolang (C) Cinogama project. 2021." << std::endl;
        std::cout << "Version: " << wo_version() << std::endl;
        std::cout << "Commit: " << wo_commit_sha() << std::endl;
        std::cout << "Date: " << wo_compile_date() << std::endl;
    }
// #endif
    wo_finish(nullptr, nullptr);

    return ret;
}
