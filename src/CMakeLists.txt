# librscene
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")

file(GLOB_RECURSE rscene_src_cpp *.cpp)

if (WIN32)
	list(APPEND rscene_system_libs "")
else()
	list(APPEND rscene_system_libs pthread dl)
endif()

if (${BUILD_SHARED_LIBS})
	add_definitions(-DRS_SHARED_LIB)
	add_library (rscene SHARED ${rscene_src_cpp})
else()
	add_definitions(-DRS_STATIC_LIB)
	if(UNIX)
		add_compile_options(-Wall -rdynamic)
	endif()
	add_library(rscene STATIC ${rscene_src_cpp})
endif()

target_link_libraries(rscene ${rscene_system_libs})