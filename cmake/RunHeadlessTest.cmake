if(NOT DEFINED MINTBOY_HEADLESS)
    message(FATAL_ERROR "MINTBOY_HEADLESS is not set")
endif()

if(NOT DEFINED MINTBOY_ROM)
    message(FATAL_ERROR "MINTBOY_ROM is not set")
endif()

if(NOT DEFINED MINTBOY_FRAMES)
    set(MINTBOY_FRAMES 600)
endif()

if(NOT DEFINED MINTBOY_EXPECTED_OUTPUT)
    set(MINTBOY_EXPECTED_OUTPUT "Passed")
endif()

execute_process(
    COMMAND "${MINTBOY_HEADLESS}" "${MINTBOY_ROM}" "${MINTBOY_FRAMES}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message("${output}")
    message("${error}")
    message(FATAL_ERROR "headless test failed with exit code ${result}")
endif()

string(FIND "${output}" "${MINTBOY_EXPECTED_OUTPUT}" passed_index)
if(passed_index EQUAL -1)
    message("${output}")
    message(FATAL_ERROR "headless test did not report ${MINTBOY_EXPECTED_OUTPUT}")
endif()

message("${output}")
