# Woolang
#
cmake_minimum_required (VERSION 3.10)

project ("Woolang")

####################################################################################################

option(BUILD_SHARED_LIBS                "Build woo as shared lib" ON)
option(WO_MAKE_OUTPUT_IN_SAME_PATH      "Build woo lib and driver and output in same path" OFF)
option(WO_BUILD_FOR_COVERAGE_TEST       "Build woo for code coverage test" OFF)
option(WO_BUILD_FOR_ASAN_TEST           "Build woo for code asan test" OFF)
option(WO_DISABLE_COMPILER              "Only keep the runtime, which helps to reduce the file size" OFF)

####################################################################################################

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

add_subdirectory("3rd/woort")

add_library(woolang_options INTERFACE)
target_compile_features(woolang_options INTERFACE cxx_std_17)
target_include_directories(woolang_options INTERFACE "include")

if (MSVC)
    target_compile_options(woolang_options INTERFACE "/utf-8")
    target_compile_definitions(woolang_options INTERFACE -D_CRT_SECURE_NO_WARNINGS)
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
    target_compile_definitions(woolang_options INTERFACE -DWO_SHARED_LIB -DWOORT_SHARED_LIB)
else ()
    target_compile_definitions(woolang_options INTERFACE -DWO_STATIC_LIB -DWOORT_STATIC_LIB)
endif ()

add_subdirectory ("src")
add_subdirectory ("driver")
