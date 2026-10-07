cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall")

project(thread_pool)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(thread_pool main.cpp thread_pool.cpp)
target_link_libraries(thread_pool PRIVATE Threads::Threads)
set_property(TARGET thread_pool PROPERTY CXX_STANDARD 11)
set_property(TARGET thread_pool PROPERTY CXX_STANDARD_REQUIRED ON)

