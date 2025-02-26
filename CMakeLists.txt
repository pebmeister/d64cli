cmake_minimum_required(VERSION 3.14)
project(d64cli)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# Fetch d64lib from GitHub
include(FetchContent)
FetchContent_Declare(
    d64lib    
    GIT_REPOSITORY https://github.com/pebmeister/d64lib
    GIT_TAG master
)
FetchContent_MakeAvailable(d64lib)

FetchContent_Declare(
    argparse    
    GIT_REPOSITORY https://github.com/p-ranav/argparse
    GIT_TAG master
)
FetchContent_MakeAvailable(argparse)

# Add executable
add_executable(d64cli main.cpp)

target_link_libraries(d64cli d64lib)
add_dependencies(d64cli argparse d64lib) 

# Ensure argparse.hpp is found
target_include_directories(
d64cli PRIVATE ${argparse_SOURCE_DIR}/include
d64cli PRIVATE ${d64lib_SOURCE_DIR}
)
target_include_directories(d64cli PRIVATE ${argparse_SOURCE_DIR}/include)
target_link_directories(d64cli PRIVATE ${d64lib_LINK_DIR})

