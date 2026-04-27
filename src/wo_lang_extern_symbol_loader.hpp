#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif
#include "wo_assert.hpp"
#include "wo_global_setting.hpp"
#include "wo_stdlib_extern_functions.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace wo
{
    class rslib_extern_symbols
    {
        inline static woort_Dylib* _current_wo_lib_handle = nullptr;

    public:
        static void init_wo_lib()
        {
            wo_assert(_current_wo_lib_handle == nullptr);

            woort_ExternLibFunc* funcs = nullptr;
            wo::stdlib::register_all(&funcs);

            _current_wo_lib_handle =
                woort_dylib_fake("woolang", funcs, NULL);
        }
        static void free_wo_lib()
        {
            wo_assert(_current_wo_lib_handle != nullptr);

            woort_dylib_unload(_current_wo_lib_handle, WOORT_DYLIB_UNREF_AND_BURY);
            _current_wo_lib_handle = nullptr;
        }

        struct extern_lib_guard
        {
            woort_Dylib* m_extern_library = nullptr;

            extern_lib_guard(const std::string& libpath, const std::string& script_path)
            {
                m_extern_library = woort_dylib_load(
                    libpath.c_str(),
                    libpath.c_str(),
                    script_path.c_str(), false);

                if (m_extern_library != nullptr)
                {
                    if (auto* entry = (woort_NativeFunction)woort_dylib_load_func(m_extern_library, "woolib_entry"))
                        entry();
                }
            }
            woort_NativeFunction load_func(const char* funcname)
            {
                if (m_extern_library != nullptr)
                    return (woort_NativeFunction)woort_dylib_load_func(m_extern_library, funcname);

                return nullptr;
            }
            ~extern_lib_guard()
            {
                if (m_extern_library != nullptr)
                {
                    if (auto* leave = (woort_NativeFunction)woort_dylib_load_func(m_extern_library, "woolib_exit"))
                        leave();

                    woort_dylib_unload(m_extern_library, WOORT_DYLIB_UNREF);
                }
            }
        };

        struct extern_lib_set
        {
            using extern_lib = std::unique_ptr<extern_lib_guard>;
            using srcpath_externlib_pairs =
                std::unordered_map<std::string, std::unordered_map<std::string, extern_lib>>;

            srcpath_externlib_pairs loaded_libsrc;

            woort_NativeFunction try_load_func_from_in(
                const char* srcpath,
                const char* libpath,
                const char* funcname)
            {
                auto& srcloadedlibs = loaded_libsrc[srcpath];

                if (auto fnd = srcloadedlibs.find(libpath);
                    fnd != srcloadedlibs.end())
                    return fnd->second->load_func(funcname);

                extern_lib elib = std::make_unique<extern_lib_guard>(libpath, srcpath);
                woort_NativeFunction const result = elib->load_func(funcname);

                srcloadedlibs.emplace(libpath, std::move(elib));

                return result;
            }

            // Collect all loaded library handles for binding to CodeEnv
            std::vector<woort_Dylib*> collect_handles() const
            {
                std::vector<woort_Dylib*> handles;
                for (const auto& [srcpath, libs] : loaded_libsrc)
                {
                    for (const auto& [libname, guard] : libs)
                    {
                        if (guard->m_extern_library != nullptr)
                            handles.push_back(guard->m_extern_library);
                    }
                }
                return handles;
            }
        };

        static woort_NativeFunction get_global_symbol(const char* symbol)
        {
            return (woort_NativeFunction)woort_dylib_load_func(
                _current_wo_lib_handle, symbol);
        }

        static woort_NativeFunction get_lib_symbol(const char* src, const char* lib, const char* symb, extern_lib_set& elibs)
        {
            return elibs.try_load_func_from_in(src, lib, symb);
        }
    };
}
