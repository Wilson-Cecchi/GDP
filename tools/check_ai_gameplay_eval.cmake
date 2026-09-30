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
    OUTPUT_VARIABLE alternate_seed_output
    ERROR_VARIABLE alternate_seed_error)
if(NOT alternate_seed_result EQUAL 0)
    message(FATAL_ERROR "Alternate seed failed: ${alternate_seed_error}")
endif()
string(REGEX MATCH "neural,open,robot,12345,[^\n]*"
    first_open_robot "${first_output}")
string(REGEX MATCH "neural,open,robot,42,[^\n]*"
    alternate_open_robot "${alternate_seed_output}")
string(REPLACE ",12345," ",SEED," first_open_robot "${first_open_robot}")
string(REPLACE ",42," ",SEED," alternate_open_robot "${alternate_open_robot}")
if(first_open_robot STREQUAL "" OR alternate_open_robot STREQUAL ""
    OR first_open_robot STREQUAL alternate_open_robot)
    message(FATAL_ERROR "Distinct seeds repeated the same open robot fixture")
endif()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 360 --duel
    RESULT_VARIABLE first_duel_result
    OUTPUT_VARIABLE first_duel_output
    ERROR_VARIABLE first_duel_error)
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 360 --duel
    RESULT_VARIABLE second_duel_result
    OUTPUT_VARIABLE second_duel_output
    ERROR_VARIABLE second_duel_error)
if(NOT first_duel_result EQUAL 0 OR NOT second_duel_result EQUAL 0
    OR NOT first_duel_output STREQUAL second_duel_output)
    message(FATAL_ERROR "AI duel evaluation is not reproducible:\n"
        "first=${first_duel_result}: ${first_duel_error}\n"
        "second=${second_duel_result}: ${second_duel_error}")
endif()
string(REGEX MATCHALL "\n(scripted|neural)," duel_rows "${first_duel_output}")
list(LENGTH duel_rows duel_row_count)
if(NOT duel_row_count EQUAL 20)
    message(FATAL_ERROR "Expected 20 duel rows, got ${duel_row_count}")
endif()
foreach(scenario ground_strafe bhop)
    foreach(archetype worker guard officer engineer robot)
        foreach(policy scripted neural)
            string(FIND "${first_duel_output}"
                "${policy},${scenario},${archetype}," position)
            if(position EQUAL -1)
                message(FATAL_ERROR "AI duel missing ${policy}/${scenario}/${archetype}")
            endif()
        endforeach()
    endforeach()
endforeach()

# This fixture must exercise real player combat and multiple physics jumps,
# not merely label a teleported player as a bunny hopper.
string(REGEX MATCH "scripted,bhop,worker,[^\n]*" bhop_row
    "${first_duel_output}")
string(REPLACE "," ";" bhop_fields "${bhop_row}")
list(GET bhop_fields 7 player_shots)
list(GET bhop_fields 8 player_hits)
list(GET bhop_fields 12 jumps)
list(GET bhop_fields 13 peak_speed)
if(player_shots LESS 1 OR player_hits LESS 1 OR jumps LESS 2
    OR peak_speed LESS_EQUAL 4.5)
    message(FATAL_ERROR "Duel did not exercise hitscan/Bhop: ${bhop_row}")
endif()
string(REGEX MATCH "scripted,ground_strafe,worker,[^\n]*" ground_row
    "${first_duel_output}")
string(REPLACE "," ";" ground_fields "${ground_row}")
list(GET ground_fields 12 ground_jumps)
list(GET ground_fields 13 ground_speed)
if(NOT ground_jumps EQUAL 0 OR ground_speed GREATER 4.5)
    message(FATAL_ERROR "Ground strafe exceeded walking rules: ${ground_row}")
endif()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 42 --frames 90 --duel
    RESULT_VARIABLE alternate_duel_result
    ERROR_VARIABLE alternate_duel_error)
if(NOT alternate_duel_result EQUAL 0)
    message(FATAL_ERROR "Alternate duel seed failed: ${alternate_duel_error}")
endif()

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --brain "guard=${GDP_BRAIN_DIR}/worker.brain" --frames 1
    RESULT_VARIABLE mismatch_result)
if(mismatch_result EQUAL 0)
    message(FATAL_ERROR "Wrong-archetype brain override was accepted")
endif()
