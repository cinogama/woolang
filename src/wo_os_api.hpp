#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif

namespace wo
{
    namespace osapi
    {
        void* loadlib   (const char* dllpath, const char* scriptpath = nullptr);
        wo_native_func_t  loadfunc(void* libhandle, const char* funcname);
        void            freelib(void* libhandle);
    }
}