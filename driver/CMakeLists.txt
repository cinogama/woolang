# rscenedriver
#

cmake_minimum_required (VERSION 3.8)

include_directories("../include")

if (BUILD_SHARED_LIBS)
	add_definitions(-DRS_SHARED_LIB)
else()
	add_definitions(-DRS_STATIC_LIB)
endif()

set_target_properties(rscenedriver PROPERTIES RELEASE_POSTFIX "")
set_target_properties(rscenedriver PROPERTIES DEBUG_POSTFIX "_debug")

add_executable(rscenedriver rs_driver.cpp)
target_link_libraries(rscenedriver rscene)