cmake_minimum_required(VERSION 3.12)

project(hamt)

include_directories(include)

file(GLOB SOURCES test/*cpp)

add_executable(test ${SOURCES})
