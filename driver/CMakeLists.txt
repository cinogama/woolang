# woodriver

cmake_minimum_required (VERSION 3.10)

include_directories("../include")

add_executable(woodriver wo_driver.cpp)

target_link_libraries(woodriver
    PRIVATE woolang_options
    PRIVATE woolang)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(woodriver PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(woodriver PROPERTIES RELWITHDEBINFO_POSTFIX "")
    set_target_properties(woodriver PROPERTIES MINSIZEREL_POSTFIX "")
    set_target_properties(woodriver PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(woodriver PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(woodriver PROPERTIES RELWITHDEBINFO_POSTFIX "32")
    set_target_properties(woodriver PROPERTIES MINSIZEREL_POSTFIX "32")
    set_target_properties(woodriver PROPERTIES DEBUG_POSTFIX "32_debug")
else()
    message(FATAL_ERROR "Unknown platform.")
endif()