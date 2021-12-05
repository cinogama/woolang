#pragma once
#ifndef RS_IMPL
#       define RS_IMPL
#       include "rs.h"
#endif
#include "rs_assert.hpp"
#include "rs_os_api.hpp"

#include <shared_mutex>

namespace rs
{
    class rslib_extern_symbols
    {
    public:
        static rs_native_func get_global_symbol(const char* symbol)
        {
            void* this_exe_handle = osapi::loadlib(nullptr);
            rs_assert(this_exe_handle);
            return osapi::loadfunc(this_exe_handle, symbol);
        }
    };
}