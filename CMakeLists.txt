cmake_minimum_required(VERSION 3.20)

project(mintboy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mintboy_core
    src/apu.cpp
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

add_executable(mintboy_headless src/headless.cpp)
target_link_libraries(mintboy_headless PRIVATE mintboy_core)

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
    tests/apu_tests.cpp
    tests/cartridge_tests.cpp
    tests/cpu_tests.cpp
    tests/memory_tests.cpp
    tests/ppu_tests.cpp
    tests/timer_tests.cpp
    tests/test_main.cpp
)

target_link_libraries(mintboy_tests PRIVATE mintboy_core)

add_test(NAME mintboy_tests COMMAND mintboy_tests)

function(add_headless_rom_test test_name rom_path frames)
    set(expected_output "Passed")
    if(ARGC GREATER 3)
        set(expected_output "${ARGV3}")
    endif()

    add_test(
        NAME "${test_name}"
        COMMAND
            ${CMAKE_COMMAND}
            "-DMINTBOY_HEADLESS=$<TARGET_FILE:mintboy_headless>"
            "-DMINTBOY_ROM=${rom_path}"
            "-DMINTBOY_FRAMES=${frames}"
            "-DMINTBOY_EXPECTED_OUTPUT=${expected_output}"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunHeadlessTest.cmake"
    )
endfunction()

set(MINTBOY_BLARGG_TEST_DIR "${PROJECT_SOURCE_DIR}/rom/tests/gb-test-roms")
set(MINTBOY_BLARGG_CPU_TEST_DIR "${MINTBOY_BLARGG_TEST_DIR}/cpu_instrs/individual")
if(EXISTS "${MINTBOY_BLARGG_CPU_TEST_DIR}")
    function(add_blargg_cpu_test test_name frames)
        add_headless_rom_test(
            "blargg_cpu_${test_name}"
            "${MINTBOY_BLARGG_CPU_TEST_DIR}/${test_name}.gb"
            "${frames}"
        )
    endfunction()

    add_blargg_cpu_test("01-special" 600)
    add_blargg_cpu_test("02-interrupts" 600)
    add_blargg_cpu_test("03-op sp,hl" 600)
    add_blargg_cpu_test("04-op r,imm" 600)
    add_blargg_cpu_test("05-op rp" 600)
    add_blargg_cpu_test("06-ld r,r" 600)
    add_blargg_cpu_test("07-jr,jp,call,ret,rst" 600)
    add_blargg_cpu_test("08-misc instrs" 600)
    add_blargg_cpu_test("09-op r,r" 600)
    add_blargg_cpu_test("10-bit ops" 3000)
    add_blargg_cpu_test("11-op a,(hl)" 3000)
else()
    message(STATUS "Blargg CPU test ROMs were not found; skipping external ROM tests")
endif()

set(MINTBOY_BLARGG_INSTR_TIMING_ROM "${MINTBOY_BLARGG_TEST_DIR}/instr_timing/instr_timing.gb")
if(EXISTS "${MINTBOY_BLARGG_INSTR_TIMING_ROM}")
    add_headless_rom_test("blargg_instr_timing" "${MINTBOY_BLARGG_INSTR_TIMING_ROM}" 1200)
else()
    message(STATUS "Blargg instruction timing test ROM was not found; skipping external instruction timing test")
endif()

set(MINTBOY_BLARGG_MEM_TIMING_TEST_DIR "${MINTBOY_BLARGG_TEST_DIR}/mem_timing/individual")
if(EXISTS "${MINTBOY_BLARGG_MEM_TIMING_TEST_DIR}")
    add_headless_rom_test("blargg_mem_timing_01-read_timing" "${MINTBOY_BLARGG_MEM_TIMING_TEST_DIR}/01-read_timing.gb" 1200)
    add_headless_rom_test("blargg_mem_timing_02-write_timing" "${MINTBOY_BLARGG_MEM_TIMING_TEST_DIR}/02-write_timing.gb" 1200)
    add_headless_rom_test("blargg_mem_timing_03-modify_timing" "${MINTBOY_BLARGG_MEM_TIMING_TEST_DIR}/03-modify_timing.gb" 1200)
else()
    message(STATUS "Blargg memory timing test ROMs were not found; skipping external memory timing tests")
endif()

set(MINTBOY_BLARGG_HALT_BUG_ROM "${MINTBOY_BLARGG_TEST_DIR}/halt_bug.gb")
if(EXISTS "${MINTBOY_BLARGG_HALT_BUG_ROM}")
    add_headless_rom_test("blargg_halt_bug" "${MINTBOY_BLARGG_HALT_BUG_ROM}" 1200 "Framebuffer hash: 0xB73934EEE037E125")
else()
    message(STATUS "Blargg HALT bug test ROM was not found; skipping external HALT bug test")
endif()

set(MINTBOY_BLARGG_DMG_SOUND_TEST_DIR "${MINTBOY_BLARGG_TEST_DIR}/dmg_sound/rom_singles")
if(EXISTS "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}")
    add_headless_rom_test("blargg_dmg_sound_01-registers" "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}/01-registers.gb" 1800)
    add_headless_rom_test("blargg_dmg_sound_02-len_ctr" "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}/02-len ctr.gb" 1800)
    add_headless_rom_test("blargg_dmg_sound_03-trigger" "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}/03-trigger.gb" 1800)
    add_headless_rom_test("blargg_dmg_sound_04-sweep" "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}/04-sweep.gb" 1800)
    add_headless_rom_test("blargg_dmg_sound_06-overflow_on_trigger" "${MINTBOY_BLARGG_DMG_SOUND_TEST_DIR}/06-overflow on trigger.gb" 1800)
else()
    message(STATUS "Blargg DMG sound test ROMs were not found; skipping external DMG sound tests")
endif()
