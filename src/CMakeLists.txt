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
    
    if (MINGW)
        add_definitions(-DWO_BUILD_WITH_MINGW=1) 
    endif()
else()
	if (NOT ANDROID)
		list(APPEND woo_system_libs pthread)
	endif()
	list(APPEND woo_system_libs dl)
endif()

add_definitions(-DWO_SRC_PATH="${PROJECT_SOURCE_DIR}")

if (${WO_SUPPORT_ASMJIT})
	add_definitions(-DWO_JIT_SUPPORT_ASMJIT=1) 
endif()

if (${WO_FORCE_GC_OBJ_THREAD_SAFETY})
	add_definitions(-DWO_FORCE_GC_OBJ_THREAD_SAFETY=1) 
endif()

if (BUILD_SHARED_LIBS)
    add_definitions(-DWO_SHARED_LIB)
    add_library (woolang SHARED ${woo_src_cpp} ${woo_src_hpp})
else()
    add_definitions(-DWO_STATIC_LIB)
    if(UNIX)
        add_compile_options(-Wall -rdynamic)
    endif()
    add_library(woolang STATIC ${woo_src_cpp} ${woo_src_hpp})
endif()

if(${WO_SUPPORT_ASMJIT})
    list(APPEND woo_system_libs asmjit)
    if (NOT WIN32)
        target_compile_options (asmjit PRIVATE -fPIC)
    endif()
endif()

target_link_libraries(woolang ${woo_system_libs})
set_target_properties(woolang PROPERTIES OUTPUT_NAME libwoo)
set_target_properties(woolang PROPERTIES PREFIX "")

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
    message(FATAL_ERROR "Unknown platform")
endif()
