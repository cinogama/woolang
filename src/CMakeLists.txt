# libwoo
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/asmjit/src")

file(GLOB_RECURSE woo_src_cpp *.cpp)
file(GLOB_RECURSE woo_src_hpp *.hpp)

if (WIN32)
	list(APPEND woo_system_libs "")
else()
	list(APPEND woo_system_libs pthread dl)
endif()

add_definitions(-DWO_SRC_PATH="${PROJECT_SOURCE_DIR}")

if(${WO_SUPPORT_ASMJIT})
	add_definitions(-DWO_JIT_SUPPORT_ASMJIT=1) 
endif()

if (BUILD_SHARED_LIBS)
	add_definitions(-DWO_SHARED_LIB)
	add_library (woolang SHARED ${woo_src_cpp} ${woo_src_hpp} enum.h)
else()
	add_definitions(-DWO_STATIC_LIB)
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
	endif()
	add_library(woolang STATIC ${woo_src_cpp} ${woo_src_hpp} enum.h)
endif()

if(${WO_SUPPORT_ASMJIT})
    list(APPEND woo_system_libs asmjit)
endif()

target_link_libraries(woolang ${woo_system_libs})
set_target_properties(woolang PROPERTIES OUTPUT_NAME libwoo)
set_target_properties(woolang PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "")
	set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "32")
	set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "32_debug")
else()
	message(FATAL_ERROR "Unknown platform")
endif()
