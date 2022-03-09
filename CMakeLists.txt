# RestorableScene
#
cmake_minimum_required (VERSION 3.8)

project ("RestorableScene")

set(CMAKE_CXX_STANDARD 17)

option(BUILD_SHARED_LIBS "Build rscene as shared lib?" ON)

# 包含子项目。
add_subdirectory ("src")

add_subdirectory ("driver")