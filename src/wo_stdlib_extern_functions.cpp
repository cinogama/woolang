#include "wo_afx.hpp"

#include "wo_internal_native_function.hpp"
#include "wo_stdlib_extern_functions.hpp"

// ======== Stdlib source strings (embedded virtual files) ========
const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = u8R"(
namespace unsafe
{
    extern("woostd_return_it_self")
    public func cast<T, F>(v: F)=> T;
}
namespace std
{
    extern("woostd_print")
    public func print(...)=> void;

    public func println(...)=> void
    {
        print(......);
        print("\n");
    }
}
)"; // TODO

const char* wo_stdlib_debug_src_path = u8"woo/std_debug.wo";
const char* wo_stdlib_debug_src_data = u8""; // TODO

const char* wo_stdlib_macro_src_path = u8"woo/std_macro.wo";
const char* wo_stdlib_macro_src_data = u8""; // TODO

const char* wo_stdlib_shell_src_path = u8"woo/std_shell.wo";
const char* wo_stdlib_shell_src_data = u8""; // TODO

namespace wo::stdlib
{
    void register_all(woort_ExternLibFunc** out_table)
    {
        static woort_ExternLibFunc funcs[] = {
            {"woostd_return_it_self", &wo::internal_native::return_it_self},
            {"woostd_print", &wo::internal_native::print},

            WOORT_EXTERN_LIB_FUNC_END,
        };
        *out_table = funcs;
    }
}
