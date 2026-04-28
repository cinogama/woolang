#include "wo_afx.hpp"

#include "wo_internal_native_function.hpp"
#include "wo_stdlib_extern_functions.hpp"

namespace wo::stdlib
{
    void register_all(woort_ExternLibFunc** out_table)
    {
        static woort_ExternLibFunc funcs[] = {
            {"woostd_return_it_self", &wo::internal_native::return_it_self},
            {"woostd_panic", &wo::internal_native::panic},
            {"woostd_print", &wo::internal_native::print},

            WOORT_EXTERN_LIB_FUNC_END,
        };
        *out_table = funcs;
    }
}
