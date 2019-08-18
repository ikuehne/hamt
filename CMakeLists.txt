cmake_minimum_required(VERSION 3.12)

project(hamt)

include_directories(include)

file(GLOB SOURCES test/*cpp)

add_executable(test ${SOURCES})

add_compile_options(
    "-Wall" "-Wpedantic" "-Wextra" "-fexceptions", "Werror"
        "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
        )
