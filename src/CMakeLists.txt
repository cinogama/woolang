# libwoo
#
cmake_minimum_required (VERSION 3.10)

include_directories("../include")
include_directories("../3rd/asmjit/src")
include_directories("../3rd/mingw-std-threads")

set(GIT_COMMIT_HASH "Unavailable")
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/wo_commit_sha.hpp
    "\"${GIT_COMMIT_HASH}\"\n")

file(GLOB_RECURSE woo_src_cpp *.cpp)
file(GLOB_RECURSE woo_src_hpp *.hpp)

if (WIN32)
    list(APPEND woo_system_libs "")
else()
    if (NOT ANDROID)
        list(APPEND woo_system_libs pthread)
    endif()
    list(APPEND woo_system_libs dl)
endif()

add_library(woolang_library_options INTERFACE)
target_compile_definitions(woolang_library_options INTERFACE -DWO_SRC_PATH="${PROJECT_SOURCE_DIR}")

if (WO_SUPPORT_ASMJIT)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_JIT_SUPPORT_ASMJIT=1) 
endif()
if (WO_DISABLE_COMPILER)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_DISABLE_COMPILER=1)
endif()
if (WO_FORCE_GC_OBJ_THREAD_SAFETY)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_FORCE_GC_OBJ_THREAD_SAFETY=1) 
endif()
if (WO_DISABLE_FUNCTION_FOR_WASM)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_DISABLE_FUNCTION_FOR_WASM=1)
endif()

if (BUILD_SHARED_LIBS)
    add_library (woolang SHARED ${woo_src_cpp} ${woo_src_hpp})
else()
    add_library(woolang STATIC ${woo_src_cpp} ${woo_src_hpp})
    if(NOT MSVC)
        target_compile_options(woolang_library_options INTERFACE -Wall -rdynamic)
    endif()
endif()

if(WO_SUPPORT_ASMJIT)
    list(APPEND woo_system_libs asmjit)
endif()

target_link_libraries(woolang 
    PRIVATE woolang_options
    PRIVATE woolang_library_options
    PRIVATE ${woo_system_libs})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(woolang PROPERTIES RELWITHDEBINFO_POSTFIX "")
    set_target_properties(woolang PROPERTIES MINSIZEREL_POSTFIX "")
    set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(woolang PROPERTIES RELWITHDEBINFO_POSTFIX "32")
    set_target_properties(woolang PROPERTIES MINSIZEREL_POSTFIX "32")
    set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "32_debug")
else()
    message(FATAL_ERROR "Unknown platform.")
endif()

set_target_properties(woolang PROPERTIES OUTPUT_NAME libwoo)
set_target_properties(woolang PROPERTIES PREFIX "")
