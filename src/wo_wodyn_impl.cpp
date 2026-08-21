#define WOODYN_IMPL

#define WO_IMPL
#include "wo.h"

wo_woort_env_locale_name_f_t wo_get_woort_env_locale_name(void)
{
    return woort_env_locale_name;
}
wo_woort_dylib_load_f_t wo_get_woort_dylib_load(void)
{
    return woort_dylib_load;
}
wo_woort_dylib_load_func_f_t wo_get_woort_dylib_load_func(void)
{
    return woort_dylib_load_func;
}
wo_woort_dylib_unload_f_t wo_get_woort_dylib_unload(void)
{
    return woort_dylib_unload;
}
