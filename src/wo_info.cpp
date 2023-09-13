#define WO_IMPL
#include "wo.h"

#define WO_VERSION(DEV,MAIN,SUB,CORRECT) ((0x##DEV##ull)<<(3*16))|((0x##MAIN##ull)<<(2*16))|((0x##SUB##ull)<<(1*16))|((0x##CORRECT##ull)<<(0*16))
#define WO_VERSION_STR(DEV,MAIN,SUB,CORRECT) #DEV "." #MAIN "." #SUB "." #CORRECT

#ifdef NDEBUG
#   define WO_DEBUG_SFX ""
#else
#   define WO_DEBUG_SFX "-debug"
#endif

constexpr wo_integer_t version = WO_VERSION(1, 12, 7, 3);
constexpr char         version_str[] = WO_VERSION_STR(1, 12, 7, 3) WO_DEBUG_SFX;

#undef WO_DEBUG_SFX
#undef WO_VERSION_STR
#undef WO_VERSION

const char* commit_sha_from_cicd =
{
    #include "wo_info.hpp"
};

wo_string_t  wo_compile_date(void)
{
    return __DATE__ " " __TIME__;
}
wo_string_t  wo_commit_sha(void)
{
    return commit_sha_from_cicd;
}
wo_string_t  wo_version(void)
{
    return version_str;
}
wo_integer_t wo_version_int(void)
{
    return version;
}
