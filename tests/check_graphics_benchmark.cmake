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
if(NOT combined_output MATCHES "shadow_capacity=0")
    file(REMOVE_RECURSE "${preference_root}")
    message(FATAL_ERROR
        "Benchmark summary omitted the active shader shadow capacity:\n${combined_output}")
endif()
if(NOT combined_output MATCHES
        "gpu_vendor=[A-Za-z0-9_.-]+ gpu_renderer=[A-Za-z0-9_.-]+ gpu_version=[A-Za-z0-9_.-]+")
    file(REMOVE_RECURSE "${preference_root}")
    message(FATAL_ERROR
        "Benchmark summary omitted the active OpenGL device identity:\n${combined_output}")
endif()

file(READ "${settings_file}" settings_after)
file(REMOVE_RECURSE "${preference_root}")
if(NOT settings_after STREQUAL settings_before)
    message(FATAL_ERROR "Benchmark modified persisted player settings")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XDG_DATA_HOME=${preference_root}"
        "${CMAKE_COMMAND}"
        -DGDP_EXECUTABLE=${GDP_EXECUTABLE}
        -DGDP_FRAMES=8
        -DGDP_SEED=12345
        -DGDP_REPEATS=3
        -DGDP_PROFILES=POTATO
        -DGDP_OFFSCREEN=ON
        -P ${CMAKE_CURRENT_LIST_DIR}/../tools/run_graphics_benchmark.cmake
    RESULT_VARIABLE repeated_result
    OUTPUT_VARIABLE repeated_output
    ERROR_VARIABLE repeated_error
    TIMEOUT 120
)
set(repeated_combined_output "${repeated_output}\n${repeated_error}")
file(REMOVE_RECURSE "${preference_root}")
if(NOT repeated_result EQUAL 0)
    message(FATAL_ERROR
        "Repeated graphics benchmark failed (${repeated_result}):\n${repeated_combined_output}")
endif()
if(NOT repeated_combined_output MATCHES
        "RUN 1/3 BENCHMARK profile=POTATO"
        OR NOT repeated_combined_output MATCHES
        "RUN 3/3 BENCHMARK profile=POTATO")
    message(FATAL_ERROR
        "Repeated benchmark omitted an individual run:\n${repeated_combined_output}")
endif()
if(NOT repeated_combined_output MATCHES
        "BENCHMARK_MEDIAN profile=POTATO seed=12345 frames=8 repeats=3"
        OR NOT repeated_combined_output MATCHES
        "draws=[0-9]+.*shadow_capacity=0.*gpu_vendor=[A-Za-z0-9_.-]+.*gpu_renderer=[A-Za-z0-9_.-]+")
    message(FATAL_ERROR
        "Repeated benchmark omitted its median or stable counters:\n${repeated_combined_output}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DGDP_EXECUTABLE=${GDP_EXECUTABLE}
        -DGDP_REPEATS=2
        -P ${CMAKE_CURRENT_LIST_DIR}/../tools/run_graphics_benchmark.cmake
    RESULT_VARIABLE even_repeats_result
    OUTPUT_VARIABLE even_repeats_output
    ERROR_VARIABLE even_repeats_error
)
if(even_repeats_result EQUAL 0
        OR NOT "${even_repeats_output}\n${even_repeats_error}" MATCHES
            "GDP_REPEATS must be odd")
    message(FATAL_ERROR "Runner accepted an ambiguous even repeat count")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DGDP_EXECUTABLE=${GDP_EXECUTABLE}
        -DGDP_PROFILES=UNKNOWN
        -P ${CMAKE_CURRENT_LIST_DIR}/../tools/run_graphics_benchmark.cmake
    RESULT_VARIABLE unknown_profile_result
    OUTPUT_VARIABLE unknown_profile_output
    ERROR_VARIABLE unknown_profile_error
)
if(unknown_profile_result EQUAL 0
        OR NOT "${unknown_profile_output}\n${unknown_profile_error}" MATCHES
            "Unknown GDP benchmark profile")
    message(FATAL_ERROR "Runner accepted an unknown graphics profile")
endif()
