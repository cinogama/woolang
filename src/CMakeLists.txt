# librscene
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")
include_directories("../3rd/asmjit/src")

file(GLOB_RECURSE rscene_src_cpp *.cpp)
file(GLOB_RECURSE rscene_src_hpp *.hpp)

if (WIN32)
	list(APPEND rscene_system_libs "")
else()
	list(APPEND rscene_system_libs pthread dl)
endif()

add_definitions(-DRS_SRC_PATH="${PROJECT_SOURCE_DIR}")

if (BUILD_SHARED_LIBS)
	add_definitions(-DRS_SHARED_LIB)
	add_library (rscene SHARED ${rscene_src_cpp} ${rscene_src_hpp} enum.h)
else()
	add_definitions(-DRS_STATIC_LIB)
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
	endif()
	add_library(rscene STATIC ${rscene_src_cpp} ${rscene_src_hpp} enum.h)
endif()

target_link_libraries(rscene asmjit ${rscene_system_libs})
set_target_properties(rscene PROPERTIES OUTPUT_NAME librscene)
set_target_properties(rscene PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set_target_properties(rscene PROPERTIES RELEASE_POSTFIX "")
	set_target_properties(rscene PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(rscene PROPERTIES RELEASE_POSTFIX "32")
	set_target_properties(rscene PROPERTIES DEBUG_POSTFIX "32_debug")
else()
	message(FATAL "Unknown platform")
endif()