if(NOT DEFINED GDP_EXECUTABLE OR NOT DEFINED GDP_BINARY_DIR)
    message(FATAL_ERROR "GDP executable and binary directory are required")
endif()

set(preference_root "${GDP_BINARY_DIR}/graphics-benchmark-test-preferences")
set(settings_directory "${preference_root}/WKC/GDP")
set(settings_file "${settings_directory}/settings.toml")
file(REMOVE_RECURSE "${preference_root}")
file(MAKE_DIRECTORY "${settings_directory}")
file(WRITE "${settings_file}"
    "version = 6\n"
    "graphics_quality = ULTRA\n"
    "vertical_sync = true\n"
)
file(READ "${settings_file}" settings_before)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XDG_DATA_HOME=${preference_root}"
        "SDL_VIDEODRIVER=offscreen"
        "SDL_AUDIODRIVER=dummy"
        "${GDP_EXECUTABLE}" --benchmark POTATO --frames 8 --seed 12345
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    TIMEOUT 120
)
set(combined_output "${output}\n${error_output}")

if(NOT result EQUAL 0)
    file(REMOVE_RECURSE "${preference_root}")
    message(FATAL_ERROR
        "GDP graphics benchmark failed (${result}):\n${combined_output}")
endif()
if(NOT combined_output MATCHES
        "Graphics quality POTATO: scale 50%, lights 2, shadows 0x0, rooms 1, detailed materials off")
    file(REMOVE_RECURSE "${preference_root}")
    message(FATAL_ERROR
        "Benchmark did not isolate the requested graphics profile:\n${combined_output}")
endif()
if(NOT combined_output MATCHES
        "BENCHMARK profile=POTATO seed=12345 resolution=1280x720 frames=8 samples=8")
    file(REMOVE_RECURSE "${preference_root}")
    message(FATAL_ERROR
        "Benchmark summary is absent or malformed:\n${combined_output}")
endif()

file(READ "${settings_file}" settings_after)
file(REMOVE_RECURSE "${preference_root}")
if(NOT settings_after STREQUAL settings_before)
    message(FATAL_ERROR "Benchmark modified persisted player settings")
endif()
