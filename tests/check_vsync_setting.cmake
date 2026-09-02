if(NOT DEFINED GDP_EXECUTABLE OR NOT DEFINED GDP_BINARY_DIR)
    message(FATAL_ERROR "GDP executable and binary directory are required")
endif()

set(preference_root "${GDP_BINARY_DIR}/vsync-test-preferences")
set(settings_directory "${preference_root}/WKC/GDP")
set(settings_file "${settings_directory}/settings.toml")
file(REMOVE_RECURSE "${preference_root}")
file(MAKE_DIRECTORY "${settings_directory}")
file(WRITE "${settings_file}"
    "version = 5\n"
    "vertical_sync = false\n"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XDG_DATA_HOME=${preference_root}"
        "SDL_VIDEODRIVER=offscreen"
        "SDL_AUDIODRIVER=dummy"
        "${GDP_EXECUTABLE}" --frames 2
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    TIMEOUT 8
)
file(REMOVE_RECURSE "${preference_root}")

set(combined_output "${output}\n${error_output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "GDP VSync settings bootstrap failed (${result}):\n${combined_output}"
    )
endif()
if(NOT combined_output MATCHES "VSync initialized disabled")
    message(FATAL_ERROR
        "Persisted VSync setting was not applied:\n${combined_output}"
    )
endif()
