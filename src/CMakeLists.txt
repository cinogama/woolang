# libwoo
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/asmjit/src")

include_directories("../3rd/mingw-std-threads")

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

if (${WO_SUPPORT_ASMJIT})
target_compile_definitions(woolang_library_options INTERFACE -DWO_JIT_SUPPORT_ASMJIT=1) 
endif()

if (${WO_FORCE_GC_OBJ_THREAD_SAFETY})
    target_compile_definitions(woolang_library_options INTERFACE -DWO_FORCE_GC_OBJ_THREAD_SAFETY=1) 
endif()

if (BUILD_SHARED_LIBS)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_SHARED_LIB)
    add_library (woolang SHARED ${woo_src_cpp} ${woo_src_hpp})
else()
    target_compile_definitions(woolang_library_options INTERFACE -DWO_STATIC_LIB)
    if(UNIX)
        target_compile_definitions(woolang_library_options -Wall -rdynamic)
    endif()
    add_library(woolang STATIC ${woo_src_cpp} ${woo_src_hpp})
endif()

if(WO_SUPPORT_ASMJIT)
    list(APPEND woo_system_libs asmjit)
endif()

target_link_libraries(woolang 
    PRIVATE woolang_options
    PRIVATE woolang_library_options
    PRIVATE ${woo_system_libs})

set_target_properties(woolang PROPERTIES OUTPUT_NAME libwoo)
set_target_properties(woolang PROPERTIES PREFIX "")
