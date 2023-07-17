cmake_minimum_required(VERSION 3.21)
project(light_thread_pool_learning)

set(CMAKE_CXX_STANDARD 23)

add_executable(light_thread_pool_learning main.cpp thread_pool.h macros.h status.h io_util.h visibility.h io_util.cpp functional.h cancel.h cancel.cpp executor.h thread_pool.cpp)
