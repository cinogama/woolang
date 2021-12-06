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
        inline static void * _current_rs_lib_handle = osapi::loadlib("librscene");
    public:
        static rs_native_func get_global_symbol(const char* symbol)
        {
            static void* this_exe_handle = osapi::loadlib(nullptr);
            rs_assert(this_exe_handle);
            auto* loaded_symb = osapi::loadfunc(this_exe_handle, symbol);
            if (!loaded_symb && _current_rs_lib_handle)
                loaded_symb = osapi::loadfunc(_current_rs_lib_handle, symbol);
            return loaded_symb;
        }
    };
}