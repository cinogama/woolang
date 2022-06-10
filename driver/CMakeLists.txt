# woodriver
#

cmake_minimum_required (VERSION 3.8)

include_directories("../include")

if (BUILD_SHARED_LIBS)
	add_definitions(-DWO_SHARED_LIB)
else()
	add_definitions(-DWO_STATIC_LIB)
endif()

add_executable(woodriver wo_driver.cpp)

set_target_properties(woodriver PROPERTIES RELEASE_POSTFIX "")
set_target_properties(woodriver PROPERTIES DEBUG_POSTFIX "_debug")
target_link_libraries(woodriver woo)