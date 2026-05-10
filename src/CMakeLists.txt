# Woolang library(libwoo).

cmake_minimum_required (VERSION 3.10)

set(GIT_COMMIT_HASH "Unavailable")
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/wo_commit_sha.hpp
    "\"${GIT_COMMIT_HASH}\"\n")

# ---- Scan woo/ directory and generate embedded file header ----
file(GLOB WOO_FILES "${PROJECT_SOURCE_DIR}/woo/*.wo")

set(WOO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(WOO_GEN_HEADER "${WOO_GEN_DIR}/wo_stdlib_embedded.inc")
file(MAKE_DIRECTORY "${WOO_GEN_DIR}")

set(EMBED_CONTENT "#pragma once\n\nstruct WooEmbeddedFile { const char* path; const char* data; };\n\n")

foreach(WOO_FILE ${WOO_FILES})
    get_filename_component(WOO_FNAME ${WOO_FILE} NAME)
    string(REPLACE "." "_" WOO_VARNAME ${WOO_FNAME})
    file(READ ${WOO_FILE} WOO_DATA)
    string(LENGTH "${WOO_DATA}" DATA_LEN)

    set(MAX_CHUNK 16000)

    string(APPEND EMBED_CONTENT
        "// woo/${WOO_FNAME}\n"
        "static const char woo_data_${WOO_VARNAME}_str[] =\n")

    set(OFFSET 0)
    while(OFFSET LESS DATA_LEN)
        string(SUBSTRING "${WOO_DATA}" ${OFFSET} ${MAX_CHUNK} CHUNK)
        string(APPEND EMBED_CONTENT
            "    u8R\"WE3MBD(${CHUNK})WE3MBD\"\n")
        math(EXPR OFFSET "${OFFSET} + ${MAX_CHUNK}")
    endwhile()

    string(APPEND EMBED_CONTENT "    ;\n\n")
endforeach()

string(APPEND EMBED_CONTENT
    "static const WooEmbeddedFile woo_embedded_files[] = {\n")
foreach(WOO_FILE ${WOO_FILES})
    get_filename_component(WOO_FNAME ${WOO_FILE} NAME)
    string(REPLACE "." "_" WOO_VARNAME ${WOO_FNAME})
    string(APPEND EMBED_CONTENT
        "    {\"woo/${WOO_FNAME}\", woo_data_${WOO_VARNAME}_str},\n")
endforeach()
string(APPEND EMBED_CONTENT "};\n")

list(LENGTH WOO_FILES WOO_COUNT)
string(APPEND EMBED_CONTENT
    "static const size_t woo_embedded_file_count = ${WOO_COUNT};\n")

file(WRITE "${WOO_GEN_HEADER}" "${EMBED_CONTENT}")

file(GLOB_RECURSE woo_src_cpp *.cpp)
file(GLOB_RECURSE woo_src_hpp *.hpp)

if (MSVC)
    list(APPEND woo_src_cpp resources.rc)
endif()

if(NOT MSVC AND NOT WO_DISABLE_FUNCTION_FOR_WASM)
    list(APPEND woo_system_libs dl)
endif()

add_library(woolang_library_options INTERFACE)
target_compile_definitions(woolang_library_options INTERFACE -DWO_SRC_PATH="${PROJECT_SOURCE_DIR}")

target_include_directories(woolang_library_options 
    INTERFACE "../include"
    INTERFACE "../3rd/asmjit/src"
    INTERFACE "../3rd/mingw-std-threads")

if (WO_SUPPORT_ASMJIT)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_JIT_SUPPORT_ASMJIT=1) 
endif()
if (WO_DISABLE_COMPILER)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_DISABLE_COMPILER=1)
endif()
if (WO_FORCE_GC_OBJ_THREAD_SAFETY)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_FORCE_GC_OBJ_THREAD_SAFETY=1) 
endif()
if (WO_DISABLE_FUNCTION_FOR_WASM)
    target_compile_definitions(woolang_library_options INTERFACE -DWO_DISABLE_FUNCTION_FOR_WASM=1)
endif()

if (BUILD_SHARED_LIBS)
    add_library (woolang SHARED ${woo_src_cpp} ${woo_src_hpp} ../include/wo.h)
else()
    add_library(woolang STATIC ${woo_src_cpp} ${woo_src_hpp} ../include/wo.h)
    if(NOT MSVC)
        target_compile_options(woolang_library_options INTERFACE -Wall)
    endif()
endif()

if (NOT MSVC)
    target_compile_options(woolang_library_options INTERFACE -pthread)
    target_link_options(woolang_library_options INTERFACE -pthread)
endif()

if(WO_SUPPORT_ASMJIT)
    list(APPEND woo_system_libs asmjit)
endif()

target_precompile_headers(woolang PRIVATE wo_afx.hpp)

target_include_directories(woolang
    PUBLIC "../3rd/woort/include"
    PRIVATE "../3rd/woort/src"
    PRIVATE "${WOO_GEN_DIR}")

target_link_libraries(woolang 
    PRIVATE woolang_options
    PRIVATE woolang_library_options
    PRIVATE woort
    PRIVATE ${woo_system_libs})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(woolang PROPERTIES RELWITHDEBINFO_POSTFIX "")
    set_target_properties(woolang PROPERTIES MINSIZEREL_POSTFIX "")
    set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(woolang PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(woolang PROPERTIES RELWITHDEBINFO_POSTFIX "32")
    set_target_properties(woolang PROPERTIES MINSIZEREL_POSTFIX "32")
    set_target_properties(woolang PROPERTIES DEBUG_POSTFIX "32_debug")
else()
    message(FATAL_ERROR "Unknown platform.")
endif()

set_target_properties(woolang PROPERTIES OUTPUT_NAME libwoo)
set_target_properties(woolang PROPERTIES PREFIX "")
