cmake_minimum_required(VERSION 3.21)
project(light_thread_pool_learning)

set(CMAKE_CXX_STANDARD 23)

add_executable(light_thread_pool_learning main.cpp thread_pool.h macros.h status.h io_util.h visibility.h io_util.cpp functional.h cancel.h cancel.cpp executor.h thread_pool.cpp)
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
target_link_libraries(light_thread_pool_learning gtest) # 这句话保证了我可以使用Gtest
