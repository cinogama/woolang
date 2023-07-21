# Woolang
#
cmake_minimum_required (VERSION 3.8)

project ("Woolang")

set(CMAKE_CXX_STANDARD 17)

if(MSVC) 
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    # TODO: Disable exception
    # add_compile_options(/EHs-)
    # add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # add_compile_options(-fno-exceptions)
endif()

option(WO_MAKE_OUTPUT_IN_SAME_PATH "Build woo lib and driver and output in same path" OFF)
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

option(BUILD_SHARED_LIBS "Build woo as shared lib" ON)
option(WO_BUILD_FOR_COVERAGE_TEST "Build woo for code coverage test" OFF)
option(WO_BUILD_FOR_ASAN_TEST "Build woo for code asan test" OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    option(WO_SUPPORT_ASMJIT "Support asmjit for compile" ON)
else()
    option(WO_SUPPORT_ASMJIT "Support asmjit for x64 only" OFF)
endif()

if (${WO_SUPPORT_ASMJIT})
    set(ASMJIT_STATIC ON CACHE BOOL "Build static-linked asmjit" FORCE)
    add_subdirectory("3rd/asmjit")
endif()

if(UNIX)
    if(WO_BUILD_FOR_COVERAGE_TEST)
        add_compile_options("-fprofile-arcs")
        add_compile_options("-ftest-coverage")
        add_link_options("-fprofile-arcs")
        add_link_options("-ftest-coverage")
    endif()
    if (WO_BUILD_FOR_ASAN_TEST)
        add_compile_options("-fsanitize=address")
        add_link_options("-fsanitize=address")
    endif()
endif()

add_subdirectory ("src")
add_subdirectory ("driver")
