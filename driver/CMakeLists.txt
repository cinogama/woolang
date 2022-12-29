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

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(woodriver PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(woodriver PROPERTIES DEBUG_POSTFIX "_debug")
else()
    set_target_properties(woodriver PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(woodriver PROPERTIES DEBUG_POSTFIX "32_debug")
endif()

target_link_libraries(woodriver woolang)

if(UNIX)
    if(WO_BUILD_FOR_COVERAGE_TEST)
        target_link_libraries(woodriver gcov)
    endif()
endif()
