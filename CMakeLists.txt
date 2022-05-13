# RestorableScene
#
cmake_minimum_required (VERSION 3.8)

project ("RestorableScene")

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
endif()

option(BUILD_SHARED_LIBS "Build rscene as shared lib" ON)

add_subdirectory ("src")
add_subdirectory ("driver")