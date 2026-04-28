#include "wo_afx.hpp"

#include "wo_internal_native_function.hpp"

namespace wo::internal_native
{
    woort_api return_it_self(void)
    {
        return woort_ret_value(0);
    }
    woort_api bad_function(void)
    {
        return woort_ret_panic("This function cannot be invoked.");
    }
    woort_api print(void)
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
            return woort_ret_panic("Failed to reserve stack.");

        const woort_Int argn = woort_int(0);
        for (woort_Int i = 1; i <= argn; ++i)
        {
            if (i != 1)
                wo::wo_stdout << " ";

            if (woort_unbox_type((woort_value)i) == WOORT_BOX_VALUE_TYPE_STRING)
                wo::wo_stdout << woort_string((woort_value)i);
            else
            {
                if (!woort_serialize_dynbox(s, (woort_value)i, WOORT_SERIALIZE_FLAG_NONE))
                    return woort_ret_panic("Out of memory.");

                wo::wo_stdout << woort_string(s);
            }

        }
        return woort_ret_void();
    }
}