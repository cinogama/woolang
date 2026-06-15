#pragma once
#include <cstddef>
#include <new>
#include <thread>
#include <algorithm>

namespace wo
{
    namespace config
    {
        inline bool ENABLE_CHECK_GRAMMAR_AND_UPDATE = false;

        /*
        * ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = false
        * --------------------------------------------------------------------
        *   When importing external functions using the extern syntax, ignore
        *   missing function symbols and replace them with the default panic
        *   function.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   This configuration item is only meaningful when used for LSP or
        *   compiled into a binary, and should not be abused. Please properly
        *   handle the situation where symbols are indeed lost.
        */
        inline bool ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = false;

        /*
        * ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = true
        * --------------------------------------------------------------------
        *   Whether to check both the divisor is zero and the overflow when the
        * division operation is performed in runtime.
        *   If enabled, compiler will generate some extra code for checking.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   Enabled by default for safety, disable it for better performance.
        *
        *   Check for integer division only.
        */
        inline bool ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = true;

        /*
        * ENABLE_SKIP_UNSAFE_CAST_INVOKE = true
        * --------------------------------------------------------------------
        *   When checking that `unsafe::cast` is ready to be called, simply
        * eliminate this function call.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_SKIP_INVOKE_UNSAFE_CAST = true;
    }
}
