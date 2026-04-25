#include "wo_afx.hpp"

#include "wo_stdlib_extern_functions.hpp"

namespace wo::stdlib
{
    void register_all(woort_ExternLibFunc** out_table)
    {
        static woort_ExternLibFunc funcs[] = {
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
