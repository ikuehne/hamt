cmake_minimum_required(VERSION 3.12)

project(hamt)

if((CMAKE_CXX_COMPILER_ID MATCHES GNU) OR (CMAKE_CXX_COMPILER_ID MATCHES Clang))
   set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -Werror -std=c++17")
   set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3")
   set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

include_directories(include)

add_library(libhamt STATIC ${CMAKE_SOURCE_DIR}/src/HAMT.cc)

add_executable(test test/test.cpp)
target_link_libraries(test libhamt)

add_executable(bench bench/bench.cpp)
target_link_libraries(bench libhamt)
