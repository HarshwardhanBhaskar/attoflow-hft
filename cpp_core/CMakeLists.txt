cmake_minimum_required(VERSION 3.20)
project(AttoFlowCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization flags
if (MSVC)
    add_compile_options(/W4 /O2 /Oi /Ot /GL /arch:AVX2)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native -flto)
endif()

# Library
add_library(attoflow_core STATIC
    src/orderbook.cpp
)

target_include_directories(attoflow_core PUBLIC
    include
)

# Benchmark executable
add_executable(benchmark_lob
    benchmark/benchmark_lob.cpp
)

target_link_libraries(benchmark_lob PRIVATE attoflow_core)
