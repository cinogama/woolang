# Woolang
#
cmake_minimum_required (VERSION 3.10)

project ("Woolang")

####################################################################################################

option(BUILD_SHARED_LIBS                "Build woo as shared lib" ON)
option(WO_MAKE_OUTPUT_IN_SAME_PATH      "Build woo lib and driver and output in same path" OFF)
option(WO_FORCE_GC_OBJ_THREAD_SAFETY    "Make sure all operation of GC object is safe, only keep gc-safe if disabled." OFF)
option(WO_BUILD_FOR_COVERAGE_TEST       "Build woo for code coverage test" OFF)
option(WO_BUILD_FOR_ASAN_TEST           "Build woo for code asan test" OFF)
option(WO_SUPPORT_ASMJIT                "Support asmjit for compile" ON)
option(WO_DISABLE_COMPILER              "Only keep the runtime, which helps to reduce the file size" OFF)
option(WO_DISABLE_FUNCTION_FOR_WASM     "Disable dynamic lib loading and some other function for WASM" OFF)

set(WO_SPECIFY_PLATFORM_TYPE "" CACHE STRING 
    "Specify the platform type for unknown platform, can be '', 'WO_PLATFORM_32' or 'WO_PLATFORM_64'")

####################################################################################################

# Check WO_SPECIFY_PLATFORM_TYPE is valid
if (NOT WO_SPECIFY_PLATFORM_TYPE STREQUAL ""
    AND NOT WO_SPECIFY_PLATFORM_TYPE STREQUAL "WO_PLATFORM_32"
    AND NOT WO_SPECIFY_PLATFORM_TYPE STREQUAL "WO_PLATFORM_64")
        message(FATAL_ERROR "WO_SPECIFY_PLATFORM_TYPE must be '', 'WO_PLATFORM_32' or 'WO_PLATFORM_64'")
endif()

if (WO_MAKE_OUTPUT_IN_SAME_PATH)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
    set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build")
endif()

if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

if (WO_SUPPORT_ASMJIT)
    set(ASMJIT_STATIC ON CACHE BOOL "Build static-linked asmjit" FORCE)
    add_subdirectory("3rd/asmjit")
    if (NOT MSVC)
        target_compile_options (asmjit PRIVATE -fPIC)
    endif()
endif()

add_library(woolang_options INTERFACE)
target_compile_features(woolang_options INTERFACE cxx_std_17)

if (MSVC)
    target_compile_options(woolang_options INTERFACE "/source-charset:utf-8")
    target_compile_definitions(woolang_options INTERFACE -D_CRT_SECURE_NO_WARNINGS)
elseif (MINGW)
    target_compile_definitions(woolang_options INTERFACE -DWO_BUILD_WITH_MINGW=1) 
else()
    target_compile_options(woolang_options INTERFACE -fno-exceptions)
endif ()

if (NOT WO_SPECIFY_PLATFORM_TYPE STREQUAL "")
    target_compile_definitions(woolang_options INTERFACE -D${WO_SPECIFY_PLATFORM_TYPE})
endif()

if(UNIX)
    if(WO_BUILD_FOR_COVERAGE_TEST)
        target_compile_options(woolang_options INTERFACE "-fprofile-arcs")
        target_compile_options(woolang_options INTERFACE "-ftest-coverage")
        target_link_options(woolang_options INTERFACE "-fprofile-arcs")
        target_link_options(woolang_options INTERFACE "-ftest-coverage")
    endif()
endif()
if (WO_BUILD_FOR_ASAN_TEST)
    if(MSVC)
        target_compile_options(woolang_options INTERFACE "/fsanitize=address")
        target_link_options(woolang_options INTERFACE "/fsanitize=address")
    else()
        target_compile_options(woolang_options INTERFACE "-fsanitize=address")
        target_link_options(woolang_options INTERFACE "-fsanitize=address")
    endif()
endif()

if (BUILD_SHARED_LIBS)
    target_compile_definitions(woolang_options INTERFACE -DWO_SHARED_LIB)
else ()
    target_compile_definitions(woolang_options INTERFACE -DWO_STATIC_LIB)
endif ()

add_subdirectory ("src")
add_subdirectory ("driver")
