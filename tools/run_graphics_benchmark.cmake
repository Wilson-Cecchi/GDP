if(NOT DEFINED GDP_EXECUTABLE OR GDP_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "GDP_EXECUTABLE must point to the GDP executable")
endif()
if(NOT EXISTS "${GDP_EXECUTABLE}")
    message(FATAL_ERROR "GDP executable not found: ${GDP_EXECUTABLE}")
endif()

if(NOT DEFINED GDP_FRAMES)
    set(GDP_FRAMES 240)
endif()
if(NOT DEFINED GDP_SEED)
    set(GDP_SEED 12345)
endif()
if(NOT DEFINED GDP_OFFSCREEN)
    set(GDP_OFFSCREEN OFF)
endif()

set(benchmark_environment "SDL_AUDIODRIVER=dummy")
if(GDP_OFFSCREEN)
    list(APPEND benchmark_environment "SDL_VIDEODRIVER=offscreen")
endif()

message(STATUS
    "GDP graphics benchmark: seed=${GDP_SEED}, frames=${GDP_FRAMES}, resolution=1280x720")
if(GDP_OFFSCREEN)
    message(STATUS
        "Offscreen mode validates the benchmark pipeline; its timings are not a hardware baseline")
endif()

foreach(profile POTATO LOW MEDIUM HIGH ULTRA)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env ${benchmark_environment}
            "${GDP_EXECUTABLE}"
            --benchmark "${profile}"
            --frames "${GDP_FRAMES}"
            --seed "${GDP_SEED}"
        RESULT_VARIABLE benchmark_result
        OUTPUT_VARIABLE benchmark_stdout
        ERROR_VARIABLE benchmark_stderr
        TIMEOUT 300
    )
    set(benchmark_output "${benchmark_stdout}\n${benchmark_stderr}")
    if(NOT benchmark_result EQUAL 0)
        message(FATAL_ERROR
            "${profile} benchmark failed with exit code ${benchmark_result}:\n${benchmark_output}")
    endif()

    string(REGEX MATCH
        "BENCHMARK profile=${profile}[^\r\n]*"
        benchmark_summary
        "${benchmark_output}")
    if(benchmark_summary STREQUAL "")
        message(FATAL_ERROR
            "${profile} benchmark did not emit a machine-readable summary:\n${benchmark_output}")
    endif()
    message(STATUS "${benchmark_summary}")
endforeach()
