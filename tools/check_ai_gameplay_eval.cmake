if(NOT DEFINED GDP_AI_EVAL OR NOT DEFINED GDP_BRAIN_DIR)
    message(FATAL_ERROR "GDP_AI_EVAL and GDP_BRAIN_DIR are required")
endif()
cmake_policy(SET CMP0007 NEW) # Preserve the empty third-role field in CSV lists.

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

# Every candidate role uses the same perceptual probe and its own brain.
foreach(role worker guard officer engineer robot)
    execute_process(
        COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
            --seed 12345 --frames 120 --investigation
            --investigation-role "${role}"
        RESULT_VARIABLE role_result
        OUTPUT_VARIABLE role_output
        ERROR_VARIABLE role_error)
    execute_process(
        COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
            --brain "${role}=${GDP_BRAIN_DIR}/${role}.brain"
            --seed 12345 --frames 120 --investigation
            --investigation-role "${role}"
        RESULT_VARIABLE role_same_result
        OUTPUT_VARIABLE role_same_output
        ERROR_VARIABLE role_same_error)
    if(NOT role_result EQUAL 0 OR NOT role_same_result EQUAL 0
        OR NOT role_output STREQUAL role_same_output)
        message(FATAL_ERROR "Investigation active/active mismatch for ${role}: "
            "${role_error} ${role_same_error}")
    endif()
    foreach(policy scripted neural)
        string(REGEX MATCH "${policy},lost_sight,${role},12345,[^\n]*"
            role_row "${role_output}")
        if(role_row STREQUAL "")
            message(FATAL_ERROR "Investigation lacks ${role}/${policy}")
        endif()
        string(REPLACE "," ";" role_fields "${role_row}")
        list(GET role_fields 6 role_sight)
        list(GET role_fields 7 role_report)
        list(GET role_fields 10 observer_investigating)
        list(GET role_fields 11 ally_investigating)
        list(GET role_fields 16 role_source)
        list(GET role_fields 21 role_unearned_refreshes)
        list(GET role_fields 22 role_unseen_updates)
        list(GET role_fields 23 role_hidden_contacts)
        if(NOT role_sight EQUAL 1 OR NOT role_report EQUAL 1
            OR observer_investigating LESS 10 OR ally_investigating LESS 10
            OR NOT role_source STREQUAL "ally_report"
            OR role_unearned_refreshes GREATER 0
            OR role_unseen_updates GREATER 0 OR role_hidden_contacts GREATER 0)
            message(FATAL_ERROR "Investigation fixture failed for ${role}: ${role_row}")
        endif()
    endforeach()
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
string(REGEX MATCH "^[^\n]*" team_header "${first_team_output}")
string(REPLACE "," ";" team_header_fields "${team_header}")
list(LENGTH team_header_fields team_header_count)
if(NOT team_header_count EQUAL 44)
    message(FATAL_ERROR "Expected 35 original and 9 role columns, got ${team_header_count}")
endif()
set(role_columns primary_shots ally_shots third_shots
    primary_player_hits ally_player_hits third_player_hits
    primary_fire_holds ally_fire_holds third_fire_holds)
foreach(index RANGE 35 43)
    math(EXPR role_index "${index} - 35")
    list(GET team_header_fields ${index} actual_column)
    list(GET role_columns ${role_index} expected_column)
    if(NOT actual_column STREQUAL expected_column)
        message(FATAL_ERROR "Team role column ${index} is ${actual_column}, expected ${expected_column}")
    endif()
endforeach()
string(REGEX MATCHALL "(scripted|neural),[^\n]*" attributed_rows
    "${first_team_output}")
foreach(row IN LISTS attributed_rows)
    string(REPLACE "," ";" fields "${row}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 44)
        message(FATAL_ERROR "Team row has ${field_count} fields: ${row}")
    endif()
    list(GET fields 13 total_shots)
    list(GET fields 14 total_hits)
    list(GET fields 18 total_holds)
    list(GET fields 35 primary_shots)
    list(GET fields 36 ally_shots)
    list(GET fields 37 third_shots)
    list(GET fields 38 primary_hits)
    list(GET fields 39 ally_hits)
    list(GET fields 40 third_hits)
    list(GET fields 41 primary_holds)
    list(GET fields 42 ally_holds)
    list(GET fields 43 third_holds)
    math(EXPR attributed_shots "${primary_shots} + ${ally_shots} + ${third_shots}")
    math(EXPR attributed_hits "${primary_hits} + ${ally_hits} + ${third_hits}")
    math(EXPR attributed_holds "${primary_holds} + ${ally_holds} + ${third_holds}")
    if(NOT attributed_shots EQUAL total_shots
        OR NOT attributed_hits EQUAL total_hits
        OR NOT attributed_holds EQUAL total_holds)
        message(FATAL_ERROR "Team role telemetry does not sum to totals: ${row}")
    endif()
endforeach()
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
if(NOT boss_field_count EQUAL 44)
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
list(GET firing_lane_fields 41 primary_fire_holds)
list(GET firing_lane_fields 39 ally_player_hits)
if(friendly_hits LESS 1 OR fire_holds LESS 1
    OR NOT primary_fire_holds EQUAL fire_holds OR ally_player_hits LESS 1)
    message(FATAL_ERROR "Firing-lane fixture did not exercise friendly fire: ${firing_lane_row}")
endif()

# Protocol 3 reports the shooter of every allied hit, kill and point of damage.
# Keep the protocol-2 CSV above byte-compatible for the active training service.
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 360 --team-attributed
    RESULT_VARIABLE attributed_result
    OUTPUT_VARIABLE attributed_output
    ERROR_VARIABLE attributed_error)
if(NOT attributed_result EQUAL 0)
    message(FATAL_ERROR "Attributed team evaluation failed: ${attributed_error}")
endif()
string(REGEX MATCH "^[^\n]*" attributed_header "${attributed_output}")
string(REPLACE "," ";" attributed_header_fields "${attributed_header}")
list(LENGTH attributed_header_fields attributed_header_count)
if(NOT attributed_header_count EQUAL 53)
    message(FATAL_ERROR "Expected 53 attributed team fields, got ${attributed_header_count}")
endif()
set(attribution_columns primary_friendly_hits ally_friendly_hits
    third_friendly_hits primary_friendly_kills ally_friendly_kills
    third_friendly_kills primary_friendly_damage ally_friendly_damage
    third_friendly_damage)
foreach(index RANGE 44 52)
    math(EXPR role_index "${index} - 44")
    list(GET attributed_header_fields ${index} actual_column)
    list(GET attribution_columns ${role_index} expected_column)
    if(NOT actual_column STREQUAL expected_column)
        message(FATAL_ERROR "Attribution column ${index} is ${actual_column}, expected ${expected_column}")
    endif()
endforeach()
string(REGEX MATCHALL "(scripted|neural),[^\n]*" attributed_rows
    "${attributed_output}")
foreach(row IN LISTS attributed_rows)
    string(REPLACE "," ";" fields "${row}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 53)
        message(FATAL_ERROR "Attributed team row has ${field_count} fields: ${row}")
    endif()
    list(GET fields 21 protocol)
    list(GET fields 15 friendly_hits)
    list(GET fields 16 friendly_kills)
    foreach(index RANGE 44 49)
        list(GET fields ${index} role_count)
        if(role_count LESS 0)
            message(FATAL_ERROR "Negative attributed count: ${row}")
        endif()
    endforeach()
    list(GET fields 44 primary_friendly_hits)
    list(GET fields 45 ally_friendly_hits)
    list(GET fields 46 third_friendly_hits)
    list(GET fields 47 primary_friendly_kills)
    list(GET fields 48 ally_friendly_kills)
    list(GET fields 49 third_friendly_kills)
    math(EXPR attributed_hits "${primary_friendly_hits} + ${ally_friendly_hits} + ${third_friendly_hits}")
    math(EXPR attributed_kills "${primary_friendly_kills} + ${ally_friendly_kills} + ${third_friendly_kills}")
    if(NOT protocol EQUAL 3 OR NOT attributed_hits EQUAL friendly_hits
        OR NOT attributed_kills EQUAL friendly_kills)
        message(FATAL_ERROR "Friendly-fire attribution does not sum to totals: ${row}")
    endif()
endforeach()

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
    list(GET long_boss_fields 35 robot_shots)
    list(GET long_boss_fields 38 robot_hits)
    if(first_hit_frame LESS 76 OR robot_player_damage LESS 1
        OR NOT long_player_damage STREQUAL robot_player_damage
        OR NOT helpers_player_damage STREQUAL "0.000"
        OR robot_shots LESS 1 OR robot_hits LESS 1)
        message(FATAL_ERROR "Boss fixture did not exercise attributed post-grace Robot damage: ${long_boss_row}")
    endif()
endforeach()

# A second fixed Boss seed keeps the Guard alive long enough to fire and hit.
# This tests source-ID attribution for a projectile from the third role.
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 2 --frames 900 --team
    RESULT_VARIABLE third_role_result
    OUTPUT_VARIABLE third_role_output
    ERROR_VARIABLE third_role_error)
if(NOT third_role_result EQUAL 0)
    message(FATAL_ERROR "Third-role Boss fixture failed: ${third_role_error}")
endif()
string(REGEX MATCH "scripted,boss_helpers,2,[^\n]*"
    third_role_row "${third_role_output}")
string(REPLACE "," ";" third_role_fields "${third_role_row}")
list(GET third_role_fields 37 guard_shots)
list(GET third_role_fields 40 guard_hits)
if(guard_shots LESS 1 OR guard_hits LESS 1)
    message(FATAL_ERROR "Boss third role did not fire and hit: ${third_role_row}")
endif()

# Investigation has its own CSV contract; legacy maps, duel and team rows
# remain unchanged. The same active Guard used as an override must reproduce
# the baseline byte for byte before candidates can be compared here.
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --seed 12345 --frames 120 --investigation
    RESULT_VARIABLE investigation_result
    OUTPUT_VARIABLE investigation_output
    ERROR_VARIABLE investigation_error)
execute_process(
    COMMAND "${GDP_AI_EVAL}" --brain-dir "${GDP_BRAIN_DIR}"
        --brain "guard=${GDP_BRAIN_DIR}/guard.brain"
        --seed 12345 --frames 120 --investigation
    RESULT_VARIABLE same_brain_result
    OUTPUT_VARIABLE same_brain_output
    ERROR_VARIABLE same_brain_error)
if(NOT investigation_result EQUAL 0 OR NOT same_brain_result EQUAL 0
    OR NOT investigation_output STREQUAL same_brain_output)
    message(FATAL_ERROR "Investigation active/active pair differs:\n"
        "baseline=${investigation_result}: ${investigation_error}\n"
        "override=${same_brain_result}: ${same_brain_error}")
endif()
string(REGEX MATCH "^[^\n]*" investigation_header "${investigation_output}")
string(REPLACE "," ";" investigation_header_fields "${investigation_header}")
list(LENGTH investigation_header_fields investigation_field_count)
if(NOT investigation_field_count EQUAL 25)
    message(FATAL_ERROR "Investigation CSV has wrong field count")
endif()
string(REGEX MATCHALL "\n(scripted|neural)," investigation_rows
    "${investigation_output}")
list(LENGTH investigation_rows investigation_row_count)
if(NOT investigation_row_count EQUAL 2)
    message(FATAL_ERROR "Expected scripted and neural investigation rows")
endif()
foreach(policy scripted neural)
    string(REGEX MATCH "${policy},lost_sight,guard,12345,[^\n]*"
        investigation_row "${investigation_output}")
    string(REPLACE "," ";" fields "${investigation_row}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 25)
        message(FATAL_ERROR "Investigation row has wrong field count: ${investigation_row}")
    endif()
    list(GET fields 6 first_sight)
    list(GET fields 7 first_report)
    list(GET fields 9 ally_sight)
    list(GET fields 10 observer_investigate)
    list(GET fields 11 ally_investigate)
    list(GET fields 16 source_at_report)
    list(GET fields 18 knowledge_age)
    list(GET fields 19 report_error)
    list(GET fields 21 unearned_refreshes)
    list(GET fields 22 unseen_updates)
    list(GET fields 23 hidden_contacts)
    if(NOT first_sight EQUAL 1 OR NOT first_report EQUAL 1
        OR NOT ally_sight EQUAL 0 OR observer_investigate LESS 1
        OR ally_investigate LESS 1 OR NOT source_at_report STREQUAL "ally_report"
        OR knowledge_age LESS_EQUAL 0 OR report_error GREATER 1.5
        OR unearned_refreshes GREATER 0 OR unseen_updates GREATER 0
        OR hidden_contacts GREATER 0)
        message(FATAL_ERROR "Investigation fixture missed visual report or leaked hidden position: ${investigation_row}")
    endif()
endforeach()
