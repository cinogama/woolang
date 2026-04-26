#include "wo_afx.hpp"

#include "wo_stdlib_extern_functions.hpp"

// ======== Stdlib source strings (embedded virtual files) ========
const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = u8R"(
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
    static woort_api woostd_print(void)
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
            return woort_ret_panic("Failed to reserve stack.");

        const woort_Int argn = woort_int(0);
        for (woort_Int i = 1; i <= argn; ++i)
        {
            if (!woort_serialize_dynbox(s, (woort_value)i, WOORT_SERIALIZE_FLAG_NONE))
                return woort_ret_panic("Out of memory.");

            if (i != 1)
                wo::wo_stdout << " ";

            wo::wo_stdout << woort_string(s);
        }
        return woort_ret_void();
    }

    void register_all(woort_ExternLibFunc** out_table)
    {
        static woort_ExternLibFunc funcs[] = {
            {"woostd_print", &woostd_print},
            // ======== TODO: String ========

            // ======== TODO: Char ========

            // ======== TODO: Array ========

            // ======== TODO: Map ========

            // ======== TODO: I/O ========

            // ======== TODO: Bit ========

            // ======== TODO: Misc ========

            WOORT_EXTERN_LIB_FUNC_END,
        };
        *out_table = funcs;
    }
}
