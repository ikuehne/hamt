cmake_minimum_required(VERSION 3.12)

project(hamt)

include_directories(include)

add_executable(test test/test.cpp)
add_executable(bench bench/bench.cpp)

add_compile_options(
    "-Wall" "-Wpedantic" "-Wextra" "-fexceptions", "Werror"
        "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
        )
