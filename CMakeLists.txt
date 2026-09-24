cmake_minimum_required(VERSION 3.20)
project(embedded_bms_qa C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BMS_COVERAGE "Instrument firmware for gcov coverage" OFF)

# ---------------------------------------------------------------- firmware
add_library(bms_core STATIC
    firmware/src/app.c
    firmware/src/bms.c
    firmware/src/cmd_handler.c
    firmware/src/crc8.c
    firmware/src/frame_reader.c
    firmware/src/protocol.c
    firmware/src/ring_buffer.c
)
target_include_directories(bms_core PUBLIC firmware/inc)
# Strict warnings on production code: warnings are treated as errors.
target_compile_options(bms_core PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)

if(BMS_COVERAGE)
    target_compile_options(bms_core PRIVATE --coverage -O0 -g)
    target_link_options(bms_core PUBLIC --coverage)
endif()

# ---------------------------------------------------------------- simulator (virtual DUT)
add_executable(bms_sim firmware/sim/main_sim.c firmware/sim/hal_sim.c)
target_include_directories(bms_sim PRIVATE firmware/sim)
target_link_libraries(bms_sim PRIVATE bms_core)
target_compile_options(bms_sim PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------- unit tests (Unity)
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)

enable_testing()
set(UNIT_TESTS
    test_crc8 test_ring_buffer test_protocol test_frame_reader
    test_bms test_cmd_handler test_app)

foreach(t IN LISTS UNIT_TESTS)
    add_executable(${t} tests/unit/${t}.c tests/unit/fake_hal.c)
    target_include_directories(${t} PRIVATE tests/unit)
    target_link_libraries(${t} PRIVATE bms_core unity)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
