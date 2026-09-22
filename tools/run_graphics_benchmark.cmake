cmake_minimum_required(VERSION 3.24)

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
if(NOT DEFINED GDP_REPEATS)
    set(GDP_REPEATS 1)
endif()
if(NOT DEFINED GDP_PROFILES)
    set(GDP_PROFILES POTATO LOW MEDIUM HIGH ULTRA)
endif()

if(NOT GDP_REPEATS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "GDP_REPEATS must be a positive odd integer")
endif()
math(EXPR repeats_remainder "${GDP_REPEATS} % 2")
if(NOT repeats_remainder EQUAL 1)
    message(FATAL_ERROR "GDP_REPEATS must be odd so the median is unambiguous")
endif()

set(valid_profiles POTATO LOW MEDIUM HIGH ULTRA)
foreach(profile IN LISTS GDP_PROFILES)
    if(NOT profile IN_LIST valid_profiles)
        message(FATAL_ERROR "Unknown GDP benchmark profile: ${profile}")
    endif()
endforeach()

function(extract_benchmark_value summary key output_variable)
    string(REGEX MATCH " ${key}=([0-9]+(\\.[0-9]+)?)" matched "${summary}")
    if(matched STREQUAL "")
        message(FATAL_ERROR
            "Benchmark summary omitted ${key}: ${summary}")
    endif()
    string(REGEX REPLACE "^ ${key}=" "" value "${matched}")
    set(${output_variable} "${value}" PARENT_SCOPE)
endfunction()

function(extract_benchmark_token summary key output_variable)
    string(REGEX MATCH " ${key}=([A-Za-z0-9_.-]+)" matched "${summary}")
    if(matched STREQUAL "")
        message(FATAL_ERROR
            "Benchmark summary omitted ${key}: ${summary}")
    endif()
    string(REGEX REPLACE "^ ${key}=" "" value "${matched}")
    set(${output_variable} "${value}" PARENT_SCOPE)
endfunction()

function(decimal_to_hundredths value output_variable)
    if(NOT value MATCHES "^([0-9]+)\\.([0-9][0-9])$")
        message(FATAL_ERROR
            "Benchmark decimal must have exactly two fractional digits: ${value}")
    endif()
    set(whole "${CMAKE_MATCH_1}")
    set(fraction "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "^0" "" fraction_integer "${fraction}")
    if(fraction_integer STREQUAL "")
        set(fraction_integer 0)
    endif()
    math(EXPR scaled "(${whole} * 100) + ${fraction_integer}")
    set(${output_variable} "${scaled}" PARENT_SCOPE)
endfunction()

function(hundredths_to_decimal value output_variable)
    math(EXPR whole "${value} / 100")
    math(EXPR fraction "${value} % 100")
    if(fraction LESS 10)
        set(fraction "0${fraction}")
    endif()
    set(${output_variable} "${whole}.${fraction}" PARENT_SCOPE)
endfunction()

set(benchmark_environment "SDL_AUDIODRIVER=dummy")
if(GDP_OFFSCREEN)
    list(APPEND benchmark_environment "SDL_VIDEODRIVER=offscreen")
endif()

message(STATUS
    "GDP graphics benchmark: seed=${GDP_SEED}, frames=${GDP_FRAMES}, repeats=${GDP_REPEATS}, resolution=1280x720")
if(GDP_OFFSCREEN)
    message(STATUS
        "Offscreen mode validates the benchmark pipeline; its timings are not a hardware baseline")
endif()

set(timing_metrics
    avg_ms p95_ms worst_ms fps over_budget_pct
    update_ms render_ms present_ms gpu_ms)
set(structural_metrics
    draws triangles rooms enemies props lights shadow_lights shadow_occluders
    low_spec culled_draws instanced_draws instances saved_draws shadow_capacity)
set(device_metrics gpu_vendor gpu_renderer gpu_version)

foreach(run RANGE 1 ${GDP_REPEATS})
    foreach(profile IN LISTS GDP_PROFILES)
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
                "${profile} benchmark run ${run} failed with exit code ${benchmark_result}:\n${benchmark_output}")
        endif()

        string(REGEX MATCH
            "BENCHMARK profile=${profile}[^\r\n]*"
            benchmark_summary
            "${benchmark_output}")
        if(benchmark_summary STREQUAL "")
            message(FATAL_ERROR
                "${profile} benchmark run ${run} did not emit a machine-readable summary:\n${benchmark_output}")
        endif()

        if(GDP_REPEATS EQUAL 1)
            message(STATUS "${benchmark_summary}")
        else()
            message(STATUS "RUN ${run}/${GDP_REPEATS} ${benchmark_summary}")
        endif()

        foreach(metric IN LISTS timing_metrics)
            extract_benchmark_value("${benchmark_summary}" "${metric}" metric_value)
            decimal_to_hundredths("${metric_value}" metric_scaled)
            set(sample_name "samples_${profile}_${metric}")
            list(APPEND ${sample_name} "${metric_scaled}")
        endforeach()

        set(structural_signature "")
        foreach(metric IN LISTS structural_metrics)
            extract_benchmark_value("${benchmark_summary}" "${metric}" metric_value)
            string(APPEND structural_signature " ${metric}=${metric_value}")
        endforeach()
        foreach(metric IN LISTS device_metrics)
            extract_benchmark_token("${benchmark_summary}" "${metric}" metric_value)
            string(APPEND structural_signature " ${metric}=${metric_value}")
        endforeach()
        set(reference_name "structure_${profile}")
        if(run EQUAL 1)
            set(${reference_name} "${structural_signature}")
        elseif(NOT "${${reference_name}}" STREQUAL "${structural_signature}")
            message(FATAL_ERROR
                "${profile} structural counters changed between benchmark runs:\n"
                "reference:${${reference_name}}\n"
                "run ${run}:${structural_signature}")
        endif()
    endforeach()
endforeach()

if(GDP_REPEATS GREATER 1)
    math(EXPR median_index "${GDP_REPEATS} / 2")
    foreach(profile IN LISTS GDP_PROFILES)
        foreach(metric IN LISTS timing_metrics)
            set(sample_name "samples_${profile}_${metric}")
            set(metric_samples "${${sample_name}}")
            list(SORT metric_samples COMPARE NATURAL ORDER ASCENDING)
            list(GET metric_samples ${median_index} metric_median_scaled)
            hundredths_to_decimal("${metric_median_scaled}" metric_median)
            set("median_${metric}" "${metric_median}")
        endforeach()
        set(reference_name "structure_${profile}")
        message(STATUS
            "BENCHMARK_MEDIAN profile=${profile} seed=${GDP_SEED} frames=${GDP_FRAMES} repeats=${GDP_REPEATS} avg_ms=${median_avg_ms} p95_ms=${median_p95_ms} worst_ms=${median_worst_ms} fps=${median_fps} over_budget_pct=${median_over_budget_pct} update_ms=${median_update_ms} render_ms=${median_render_ms} present_ms=${median_present_ms} gpu_ms=${median_gpu_ms}${${reference_name}}")
    endforeach()
endif()
