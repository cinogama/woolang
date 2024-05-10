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
target_compile_features(woodriver PUBLIC cxx_std_17)
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
    message(FATAL_ERROR "Unknown platform")
endif()

target_link_libraries(woodriver woolang)

if(NOT MSVC AND NOT MINGW) 
    target_compile_options(woodriver PUBLIC -fno-exceptions)
endif()