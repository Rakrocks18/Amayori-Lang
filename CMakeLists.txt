cmake_minimum_required(VERSION 3.13.4)
project(amayori-llvm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# Specify the path to your LLVM build
set(LLVM_DIR "/home/vivek/llvm-build/lib/cmake/llvm")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Add your source files here

file(GLOB SOURCES "src/*.cpp")
add_executable(amayori-llvm ${SOURCES})

# Link against LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(amayori-llvm ${llvm_libs})

add_subdirectory(src/amyr-borrow-check)