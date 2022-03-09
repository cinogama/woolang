# CMakeList.txt: RestorableScene 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

include_directories("../include")

file(GLOB_RECURSE rscene_src_cpp *.cpp)

# 将源代码添加到此项目的可执行文件。
if (BUILD_SHARED_LIBS)
	add_definitions(-DRS_SHARED_LIB)
	add_library (rscene SHARED ${rscene_src_cpp})
else()
	add_definitions(-DRS_STATIC_LIB)
	add_library(rscene STATIC ${rscene_src_cpp})
endif()

# TODO: 如有需要，请添加测试并安装目标。
