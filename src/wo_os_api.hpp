#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif

#include <sys/stat.h>
#include <string>

namespace wo
{
    namespace osapi
    {
        void* loadlib(const char* dllpath, const char* scriptpath_may_null);
        void* loadfunc(void* libhandle, const char* funcname);
        void  freelib(void* libhandle);
    }

    void normalize_path(std::string* inout_path);
}