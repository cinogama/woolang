# woodriver
#

cmake_minimum_required (VERSION 3.8)

include_directories("../include")

add_executable(woodriver wo_driver.cpp)

target_link_libraries(woodriver
    PRIVATE woolang_options
    PRIVATE woolang)
