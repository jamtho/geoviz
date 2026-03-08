cmake_minimum_required(VERSION 3.16)
project(geoviz C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ----- raylib via FetchContent -----
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# ----- cJSON (vendored) -----
add_library(cjson STATIC third_party/cjson/cJSON.c)
target_include_directories(cjson PUBLIC third_party/cjson)

# ----- DuckDB (vendored amalgamation, compiled as C++) -----
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/duckdb/duckdb.cpp")
    add_library(duckdb STATIC third_party/duckdb/duckdb.cpp)
    target_include_directories(duckdb PUBLIC third_party/duckdb)
    set_target_properties(duckdb PROPERTIES LINKER_LANGUAGE CXX)
    target_compile_definitions(duckdb PUBLIC DUCKDB_BUILD_LIBRARY)
    if(MSVC)
        target_compile_options(duckdb PRIVATE /bigobj)
    endif()
    if(WIN32)
        target_compile_definitions(duckdb PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(duckdb PRIVATE ws2_32 rstrtmgr bcrypt)
    else()
        target_link_libraries(duckdb PRIVATE pthread dl)
    endif()
else()
    message(FATAL_ERROR
        "DuckDB amalgamation not found at third_party/duckdb/duckdb.cpp\n"
        "Run: python scripts/download_deps.py")
endif()

# ----- Main executable -----
add_executable(geoviz
    src/main.c
    src/spec.c
    src/data.c
    src/tiles.c
    src/render.c
    src/colormap.c
)

target_include_directories(geoviz PRIVATE src)
target_link_libraries(geoviz PRIVATE raylib cjson duckdb)

# Platform-specific HTTP implementation
if(WIN32)
    target_sources(geoviz PRIVATE src/http_winhttp.c)
    target_link_libraries(geoviz PRIVATE winhttp ws2_32 winmm)
else()
    find_package(CURL REQUIRED)
    target_sources(geoviz PRIVATE src/http_curl.c)
    target_link_libraries(geoviz PRIVATE CURL::libcurl pthread dl m)
endif()

# Compiler warnings and platform fixes
if(MSVC)
    target_compile_options(geoviz PRIVATE /W3)
else()
    target_compile_options(geoviz PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ----- Tests -----
enable_testing()

# Test: colormap
add_executable(test_colormap tests/test_colormap.c src/colormap.c)
target_include_directories(test_colormap PRIVATE src)
add_test(NAME test_colormap COMMAND test_colormap)

# Test: mercator
add_executable(test_mercator tests/test_mercator.c)
target_include_directories(test_mercator PRIVATE src)
add_test(NAME test_mercator COMMAND test_mercator)

# Test: spec parsing
add_executable(test_spec tests/test_spec.c src/spec.c src/colormap.c)
target_include_directories(test_spec PRIVATE src)
target_link_libraries(test_spec PRIVATE cjson)
add_test(NAME test_spec COMMAND test_spec)
