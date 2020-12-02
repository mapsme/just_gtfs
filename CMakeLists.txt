cmake_minimum_required(VERSION 3.6)

project(just_gtfs LANGUAGES CXX VERSION 0.1)

include_directories(include)
include_directories(doctest/doctest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

enable_testing()

add_library(just_gtfs INTERFACE)
target_include_directories(just_gtfs INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(tests)
add_subdirectory(benchmarks)
