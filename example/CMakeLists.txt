cmake_minimum_required(VERSION 3.16)
project(depthlog_example LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(depthlog SOURCE_DIR ..)
FetchContent_MakeAvailable(depthlog)

add_executable(example_proj example.cpp)
target_link_libraries(example_proj PRIVATE depthlog::depthlog)
target_compile_features(example_proj PRIVATE cxx_std_17)

