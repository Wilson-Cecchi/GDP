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

execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 360 --team
    RESULT_VARIABLE first_team_result
    OUTPUT_VARIABLE first_team_output
    ERROR_VARIABLE first_team_error)
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 360 --team
    RESULT_VARIABLE second_team_result
    OUTPUT_VARIABLE second_team_output
    ERROR_VARIABLE second_team_error)
if(NOT first_team_result EQUAL 0 OR NOT second_team_result EQUAL 0
    OR NOT first_team_output STREQUAL second_team_output)
    message(FATAL_ERROR "AI team evaluation is not reproducible:\n"
        "first=${first_team_result}: ${first_team_error}\n"
        "second=${second_team_result}: ${second_team_error}")
endif()
string(REGEX MATCHALL "\n(scripted|neural)," team_rows "${first_team_output}")
list(LENGTH team_rows team_row_count)
if(NOT team_row_count EQUAL 8)
    message(FATAL_ERROR "Expected 8 mixed-team rows, got ${team_row_count}")
endif()
foreach(scenario firing_lane officer_guard engineer_guard boss_helpers)
    foreach(policy scripted neural)
        string(FIND "${first_team_output}"
            "${policy},${scenario},12345," position)
        if(position EQUAL -1)
            message(FATAL_ERROR "AI team evaluation missing ${policy}/${scenario}")
        endif()
    endforeach()
endforeach()
string(REGEX MATCH "scripted,boss_helpers,12345,[^\n]*"
    boss_row "${first_team_output}")
string(REPLACE "," ";" boss_fields "${boss_row}")
list(LENGTH boss_fields boss_field_count)
if(NOT boss_field_count EQUAL 35)
    message(FATAL_ERROR "Boss/team protocol-2 field count changed: ${boss_row}")
endif()
list(GET boss_fields 3 boss_primary)
list(GET boss_fields 4 boss_ally)
list(GET boss_fields 21 boss_protocol)
list(GET boss_fields 22 boss_third)
list(GET boss_fields 27 boss_initial_distance)
list(GET boss_fields 30 boss_long_range)
if(NOT boss_primary STREQUAL "robot" OR NOT boss_ally STREQUAL "officer"
    OR NOT boss_third STREQUAL "guard" OR NOT boss_protocol EQUAL 2
    OR boss_initial_distance LESS 12 OR boss_long_range LESS 1)
    message(FATAL_ERROR "Boss fixture did not exercise the wide-room encounter: ${boss_row}")
endif()
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 42 --frames 360 --team
    RESULT_VARIABLE alternate_team_result
    OUTPUT_VARIABLE alternate_team_output
    ERROR_VARIABLE alternate_team_error)
if(NOT alternate_team_result EQUAL 0)
    message(FATAL_ERROR "Alternate Boss seed failed: ${alternate_team_error}")
endif()
string(REGEX MATCH "scripted,boss_helpers,42,[^\n]*"
    alternate_boss_row "${alternate_team_output}")
string(REPLACE "," ";" alternate_boss_fields "${alternate_boss_row}")
list(GET alternate_boss_fields 27 alternate_boss_initial_distance)
if(boss_initial_distance STREQUAL alternate_boss_initial_distance)
    message(FATAL_ERROR "Distinct Boss seeds repeated the same sightline")
endif()
string(REGEX MATCH "scripted,firing_lane,12345,[^\n]*"
    firing_lane_row "${first_team_output}")
string(REPLACE "," ";" firing_lane_fields "${firing_lane_row}")
list(GET firing_lane_fields 15 friendly_hits)
list(GET firing_lane_fields 18 fire_holds)
if(friendly_hits LESS 1 OR fire_holds LESS 1)
    message(FATAL_ERROR "Firing-lane fixture did not exercise friendly fire: ${firing_lane_row}")
endif()

# The long Boss fixture must expose Robot damage after the room-entry grace,
# while keeping the same scripted setup for both policies.
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 900 --team
    RESULT_VARIABLE long_boss_result
    OUTPUT_VARIABLE long_boss_output
    ERROR_VARIABLE long_boss_error)
if(NOT long_boss_result EQUAL 0)
    message(FATAL_ERROR "Long Boss fixture failed: ${long_boss_error}")
endif()
foreach(policy scripted neural)
    string(REGEX MATCH "${policy},boss_helpers,12345,[^\n]*"
        long_boss_row "${long_boss_output}")
    string(REPLACE "," ";" long_boss_fields "${long_boss_row}")
    list(GET long_boss_fields 9 long_player_damage)
    list(GET long_boss_fields 26 first_hit_frame)
    list(GET long_boss_fields 33 robot_player_damage)
    list(GET long_boss_fields 34 helpers_player_damage)
    if(first_hit_frame LESS 76 OR robot_player_damage LESS 1
        OR NOT long_player_damage STREQUAL robot_player_damage
        OR NOT helpers_player_damage STREQUAL "0.000")
        message(FATAL_ERROR "Boss fixture did not exercise attributed post-grace Robot damage: ${long_boss_row}")
    endif()
endforeach()
