cmake_minimum_required(VERSION 3.10)
project(amayori)

# Specify the path to your LLVM installation (adjust this path if necessary)
set(LLVM_DIR "/llvm-build/build/lib/cmake/llvm")

# Set the compilers to use clang
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Use lld as the linker
set(CMAKE_C_LINKER "lld")
set(CMAKE_CXX_LINKER "lld")

# Alternatively, you can pass linker flags
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld")




# Find the LLVM package
find_package(LLVM REQUIRED CONFIG)

# Include LLVM headers
include_directories("/llvm-build/build/include")

# Link against LLVM libraries
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
link_directories("/home/rakshit_shah/llvm-build/build/lib")

# Add your executable
add_executable(amayori src/main.cpp)


target_link_libraries(amayori ${LLVM_LIBS})
# Link the LLVM libraries required by your project
target_link_libraries(amayori ${LLVM_LIBS} ${LLVM_ALL_LIBS})

# Link against MLIR if you're using MLIR-specific APIs
target_link_libraries(amayori)
