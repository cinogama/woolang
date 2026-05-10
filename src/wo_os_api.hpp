#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif

#include <string>

namespace wo
{
    std::string get_file_loc(std::string path);
    void normalize_path(std::string* inout_path);
}
