cmake_minimum_required(VERSION 3.20)

project(mintboy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mintboy_core
    src/cartridge.cpp
    src/cpu.cpp
    src/memory.cpp
)

target_include_directories(mintboy_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(mintboy_core
    PRIVATE
        $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>
)

add_executable(mintboy src/main.cpp)
target_link_libraries(mintboy PRIVATE mintboy_core)

enable_testing()

add_executable(mintboy_tests
    tests/cartridge_tests.cpp
    tests/cpu_tests.cpp
    tests/test_main.cpp
)

target_link_libraries(mintboy_tests PRIVATE mintboy_core)

add_test(NAME mintboy_tests COMMAND mintboy_tests)
