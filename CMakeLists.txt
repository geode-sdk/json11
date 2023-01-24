cmake_minimum_required(VERSION 3.10)
project(json11 CXX)

option(JSON11_BUILD_TESTS "Build unit tests" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(json11 json11.cpp)
target_include_directories(json11 PUBLIC include)

if (JSON11_BUILD_TESTS)
  add_executable(json11_test test.cpp)
  target_link_libraries(json11_test json11)
endif()
