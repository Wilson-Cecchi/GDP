if(NOT DEFINED GDP_EXECUTABLE)
    message(FATAL_ERROR "GDP_EXECUTABLE is required")
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
