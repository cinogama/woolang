#include "wo_afx.hpp"

#include "wo_internal_native_function.hpp"

namespace wo::internal_native
{
    woort_api return_it_self()
    {
        return woort_ret_value(0);
    }
    woort_api bad_function()
    {
        return woort_ret_panic("This function cannot be invoked.");
    }
}