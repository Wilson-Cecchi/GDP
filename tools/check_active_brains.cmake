if(NOT DEFINED GDP_EXECUTABLE OR NOT DEFINED GDP_BRAIN_DIR)
    message(FATAL_ERROR "GDP_EXECUTABLE and GDP_BRAIN_DIR are required")
endif()

execute_process(
    COMMAND "${GDP_EXECUTABLE}" --headless --frames 3 --seed 42
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Headless GDP failed: ${error_output}")
endif()

foreach(type worker guard officer engineer robot)
    if(NOT error_output MATCHES "Trained enemy brain active: ${type}")
        message(FATAL_ERROR "Bundled ${type} brain did not activate: ${error_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${GDP_EXECUTABLE}" --headless --frames 3 --seed 42
        --scripted-enemies
    RESULT_VARIABLE scripted_result
    ERROR_VARIABLE scripted_error
)
if(NOT scripted_result EQUAL 0
    OR NOT scripted_error MATCHES "Scripted enemy AI selected"
    OR scripted_error MATCHES "Trained enemy brain active:")
    message(FATAL_ERROR "Scripted AI fallback did not activate cleanly: ${scripted_error}")
endif()

execute_process(
    COMMAND "${GDP_EXECUTABLE}" --headless --frames 3 --seed 42
        --test-brain-dir "${GDP_BRAIN_DIR}"
    RESULT_VARIABLE test_result
    ERROR_VARIABLE test_error
)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "Test-brain mode failed: ${test_error}")
endif()
foreach(type worker guard officer engineer robot)
    if(type STREQUAL "worker")
        set(expected_generation 1820)
    elseif(type STREQUAL "robot")
        set(expected_generation 2030)
    else()
        set(expected_generation 200)
    endif()
    if(NOT test_error MATCHES "Test enemy brain active: ${type} generation ${expected_generation}")
        message(FATAL_ERROR "Test-brain ${type} did not activate: ${test_error}")
    endif()
endforeach()

execute_process(
    COMMAND "${GDP_EXECUTABLE}" --headless --frames 3 --seed 42
        --test-brain-dir "${GDP_BRAIN_DIR}/does-not-exist"
    RESULT_VARIABLE missing_result
    ERROR_VARIABLE missing_error
)
if(missing_result EQUAL 0
    OR NOT missing_error MATCHES "Test brain directory needs five valid matching")
    message(FATAL_ERROR "Incomplete test-brain directory was accepted: ${missing_error}")
endif()

execute_process(
    COMMAND "${GDP_EXECUTABLE}" --headless --frames 3 --seed 42
        --scripted-enemies --test-brain-dir "${GDP_BRAIN_DIR}"
    RESULT_VARIABLE conflict_result
    ERROR_VARIABLE conflict_error
)
if(conflict_result EQUAL 0
    OR NOT conflict_error MATCHES "cannot be combined with other enemy AI modes")
    message(FATAL_ERROR "Conflicting enemy AI modes were accepted: ${conflict_error}")
endif()
