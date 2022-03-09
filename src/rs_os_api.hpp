#pragma once
#ifndef RS_IMPL
#       define RS_IMPL
#       include "rs.h"
#endif

namespace rs
{
    namespace osapi
    {
        void* loadlib   (const char* dllpath, const char* scriptpath = nullptr);
        rs_native_func  loadfunc(void* libhandle, const char* funcname);
        void            freelib(void* libhandle);
    }
}