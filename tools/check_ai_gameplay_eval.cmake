if(NOT DEFINED GDP_AI_EVAL OR NOT DEFINED GDP_BRAIN_DIR)
    message(FATAL_ERROR "GDP_AI_EVAL and GDP_BRAIN_DIR are required")
endif()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 90
    RESULT_VARIABLE first_result
    OUTPUT_VARIABLE first_output
    ERROR_VARIABLE first_error)
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 90
    RESULT_VARIABLE second_result
    OUTPUT_VARIABLE second_output
    ERROR_VARIABLE second_error)
if(NOT first_result EQUAL 0 OR NOT second_result EQUAL 0
    OR NOT first_output STREQUAL second_output)
    message(FATAL_ERROR "AI gameplay evaluation is not reproducible:\n"
        "first=${first_result}: ${first_error}\n"
        "second=${second_result}: ${second_error}")
endif()

string(REGEX MATCHALL "\n(scripted|neural)," data_rows "${first_output}")
list(LENGTH data_rows row_count)
if(NOT row_count EQUAL 42)
    message(FATAL_ERROR "Expected 42 evaluation rows, got ${row_count}")
endif()

foreach(row
        "scripted,open,worker,"
        "neural,center_cover,worker,"
        "neural,elevated,robot,"
        "neural,group,guard,"
        "neural,boss_cover,robot,")
    string(FIND "${first_output}" "${row}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "AI gameplay evaluation missing ${row}")
    endif()
endforeach()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 42 --frames 90
    RESULT_VARIABLE alternate_seed_result
    ERROR_VARIABLE alternate_seed_error)
if(NOT alternate_seed_result EQUAL 0)
    message(FATAL_ERROR "Alternate seed failed: ${alternate_seed_error}")
endif()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --brain "guard=${GDP_BRAIN_DIR}/worker.brain" --frames 1
    RESULT_VARIABLE mismatch_result)
if(mismatch_result EQUAL 0)
    message(FATAL_ERROR "Wrong-archetype brain override was accepted")
endif()
