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

add_executable(mintboy_info src/main.cpp)
target_link_libraries(mintboy_info PRIVATE mintboy_core)

option(MINTBOY_BUILD_SDL "Build the SDL2 frontend when SDL2 is available" ON)

if(MINTBOY_BUILD_SDL)
    find_package(SDL2 CONFIG QUIET)

    if(TARGET SDL2::SDL2)
        set(MINTBOY_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(MINTBOY_SDL2_TARGET SDL2::SDL2-static)
    else()
        find_package(SDL2 QUIET)
        if(SDL2_FOUND)
            set(MINTBOY_SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
            set(MINTBOY_SDL2_LIBRARIES ${SDL2_LIBRARIES})
        endif()
    endif()

    if(MINTBOY_SDL2_TARGET OR SDL2_FOUND)
        add_executable(mintboy src/app_sdl.cpp)
        target_link_libraries(mintboy PRIVATE mintboy_core)
        target_compile_definitions(mintboy PRIVATE SDL_MAIN_HANDLED)

        if(MINTBOY_SDL2_TARGET)
            target_link_libraries(mintboy PRIVATE ${MINTBOY_SDL2_TARGET})
            if(TARGET SDL2::SDL2main)
                target_link_libraries(mintboy PRIVATE SDL2::SDL2main)
            endif()
        else()
            target_include_directories(mintboy PRIVATE ${MINTBOY_SDL2_INCLUDE_DIRS})
            target_link_libraries(mintboy PRIVATE ${MINTBOY_SDL2_LIBRARIES})
        endif()
    else()
        message(STATUS "SDL2 was not found; building CLI-only mintboy target")
        add_executable(mintboy src/main.cpp)
        target_link_libraries(mintboy PRIVATE mintboy_core)
    endif()
else()
    add_executable(mintboy src/main.cpp)
    target_link_libraries(mintboy PRIVATE mintboy_core)
endif()

enable_testing()

add_executable(mintboy_tests
    tests/cartridge_tests.cpp
    tests/cpu_tests.cpp
    tests/memory_tests.cpp
    tests/test_main.cpp
)

target_link_libraries(mintboy_tests PRIVATE mintboy_core)

add_test(NAME mintboy_tests COMMAND mintboy_tests)
