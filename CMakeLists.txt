cmake_minimum_required(VERSION 3.24)

project(
    GDP
    VERSION 0.1.0
    DESCRIPTION "GDP procedural retro-industrial FPS"
    LANGUAGES CXX
)

include(FetchContent)

option(GDP_FETCH_SDL3 "Download SDL3 when no system package is available" ON)
option(GDP_BUILD_TESTS "Build and register GDP tests" ON)
option(GDP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(SDL3 3.4 CONFIG QUIET)

if(NOT SDL3_FOUND)
    if(NOT GDP_FETCH_SDL3)
        message(FATAL_ERROR "SDL3 was not found and GDP_FETCH_SDL3 is OFF")
    endif()

    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "Do not build SDL3 as a static library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Do not build SDL3 tests" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL3 test library" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL3 examples" FORCE)

    FetchContent_Declare(
        SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
        URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

find_package(OpenGL REQUIRED)

function(gdp_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
    endif()
endfunction()

function(gdp_enable_sanitizers target_name)
    if(NOT GDP_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target_name}
            PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    else()
        message(WARNING "GDP_ENABLE_SANITIZERS is unsupported by this compiler")
    endif()
endfunction()

add_library(
    gdp_core
    STATIC
    src/core/Hash.cpp
    src/core/Input.cpp
    src/core/Log.cpp
    src/core/Random.cpp
    src/core/Seed.cpp
    src/core/Time.cpp
)

target_compile_features(gdp_core PUBLIC cxx_std_20)
target_include_directories(gdp_core PUBLIC src)
target_link_libraries(gdp_core PUBLIC SDL3::SDL3)
gdp_enable_warnings(gdp_core)
gdp_enable_sanitizers(gdp_core)

add_library(
    gdp_entities
    STATIC
    src/entities/EntityManager.cpp
)

target_compile_features(gdp_entities PUBLIC cxx_std_20)
target_include_directories(gdp_entities PUBLIC src)
target_link_libraries(gdp_entities PUBLIC gdp_core)
gdp_enable_warnings(gdp_entities)
gdp_enable_sanitizers(gdp_entities)

add_library(
    gdp_audio
    STATIC
    src/audio/AudioSystem.cpp
    src/audio/ProceduralAudio.cpp
)

target_compile_features(gdp_audio PUBLIC cxx_std_20)
target_include_directories(gdp_audio PUBLIC src)
target_link_libraries(gdp_audio PUBLIC gdp_core SDL3::SDL3)
gdp_enable_warnings(gdp_audio)
gdp_enable_sanitizers(gdp_audio)

add_library(
    gdp_gameplay
    STATIC
    src/gameplay/Collision.cpp
    src/gameplay/Combat.cpp
    src/gameplay/Door.cpp
    src/gameplay/Enemy.cpp
    src/gameplay/Feedback.cpp
    src/gameplay/Loot.cpp
    src/gameplay/MainMenu.cpp
    src/gameplay/Pause.cpp
    src/gameplay/Player.cpp
    src/gameplay/Progression.cpp
    src/gameplay/Props.cpp
    src/gameplay/RoomEvents.cpp
    src/gameplay/RoomTraversal.cpp
    src/gameplay/Session.cpp
    src/gameplay/Weapon.cpp
)

target_compile_features(gdp_gameplay PUBLIC cxx_std_20)
target_include_directories(gdp_gameplay PUBLIC src)
target_link_libraries(gdp_gameplay PUBLIC gdp_core gdp_entities)
gdp_enable_warnings(gdp_gameplay)
gdp_enable_sanitizers(gdp_gameplay)

add_library(
    gdp_renderer
    STATIC
    src/procedural/AssetRecipe.cpp
    src/procedural/EnemyGenerator.cpp
    src/procedural/MeshGenerator.cpp
    src/procedural/RoomMaterialGenerator.cpp
    src/procedural/TextureGenerator.cpp
    src/render/Camera.cpp
    src/render/Framebuffer.cpp
    src/render/GlApi.cpp
    src/render/Mesh.cpp
    src/render/Renderer.cpp
    src/render/Shader.cpp
    src/render/Texture.cpp
)

target_compile_features(gdp_renderer PUBLIC cxx_std_20)
target_include_directories(gdp_renderer PUBLIC src)
target_link_libraries(gdp_renderer PUBLIC gdp_core OpenGL::GL)
gdp_enable_warnings(gdp_renderer)
gdp_enable_sanitizers(gdp_renderer)

add_library(
    gdp_world
    STATIC
    src/world/Connector.cpp
    src/world/Corridor.cpp
    src/world/EventGenerator.cpp
    src/world/LightingGenerator.cpp
    src/world/LootGenerator.cpp
    src/world/MapGraph.cpp
    src/world/PropGenerator.cpp
    src/world/RoomModule.cpp
    src/world/World.cpp
    src/world/WorldEntities.cpp
    src/world/WorldGenerator.cpp
    src/world/WorldValidation.cpp
)

target_compile_features(gdp_world PUBLIC cxx_std_20)
target_include_directories(gdp_world PUBLIC src)
target_link_libraries(gdp_world PUBLIC gdp_core gdp_entities)
gdp_enable_warnings(gdp_world)
gdp_enable_sanitizers(gdp_world)

add_executable(
    gdp
    src/main.cpp
    src/core/Application.cpp
)

target_link_libraries(gdp PRIVATE gdp_audio gdp_core gdp_entities gdp_gameplay gdp_renderer gdp_world)
gdp_enable_warnings(gdp)
gdp_enable_sanitizers(gdp)

if(GDP_BUILD_TESTS)
    include(CTest)

    add_executable(gdp_core_tests tests/test_core.cpp)
    target_link_libraries(gdp_core_tests PRIVATE gdp_core)
    gdp_enable_warnings(gdp_core_tests)
    gdp_enable_sanitizers(gdp_core_tests)

    add_test(NAME core_systems COMMAND gdp_core_tests)
    set_tests_properties(core_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_audio_tests tests/test_audio.cpp)
    target_link_libraries(gdp_audio_tests PRIVATE gdp_audio)
    gdp_enable_warnings(gdp_audio_tests)
    gdp_enable_sanitizers(gdp_audio_tests)

    add_test(NAME procedural_audio_systems COMMAND gdp_audio_tests)
    set_tests_properties(procedural_audio_systems PROPERTIES TIMEOUT 15)

    add_executable(gdp_renderer_tests tests/test_renderer.cpp)
    target_link_libraries(gdp_renderer_tests PRIVATE gdp_renderer)
    gdp_enable_warnings(gdp_renderer_tests)
    gdp_enable_sanitizers(gdp_renderer_tests)

    add_test(NAME renderer_cpu_systems COMMAND gdp_renderer_tests)
    set_tests_properties(renderer_cpu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_asset_tests tests/test_assets.cpp)
    target_link_libraries(gdp_asset_tests PRIVATE gdp_renderer)
    gdp_enable_warnings(gdp_asset_tests)
    gdp_enable_sanitizers(gdp_asset_tests)

    add_test(NAME procedural_asset_systems COMMAND gdp_asset_tests)
    set_tests_properties(procedural_asset_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_enemy_generation_tests tests/test_enemy_generation.cpp)
    target_link_libraries(gdp_enemy_generation_tests PRIVATE gdp_renderer)
    gdp_enable_warnings(gdp_enemy_generation_tests)
    gdp_enable_sanitizers(gdp_enemy_generation_tests)

    add_test(NAME enemy_generation_systems COMMAND gdp_enemy_generation_tests)
    set_tests_properties(enemy_generation_systems PROPERTIES TIMEOUT 60)

    add_executable(gdp_room_material_tests tests/test_room_materials.cpp)
    target_link_libraries(gdp_room_material_tests PRIVATE gdp_renderer)
    gdp_enable_warnings(gdp_room_material_tests)
    gdp_enable_sanitizers(gdp_room_material_tests)

    add_test(NAME room_material_systems COMMAND gdp_room_material_tests)
    set_tests_properties(room_material_systems PROPERTIES TIMEOUT 120)

    add_executable(gdp_gameplay_tests tests/test_gameplay.cpp)
    target_link_libraries(gdp_gameplay_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_gameplay_tests)
    gdp_enable_sanitizers(gdp_gameplay_tests)

    add_test(NAME gameplay_systems COMMAND gdp_gameplay_tests)
    set_tests_properties(gameplay_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_feedback_tests tests/test_feedback.cpp)
    target_link_libraries(gdp_feedback_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_feedback_tests)
    gdp_enable_sanitizers(gdp_feedback_tests)

    add_test(NAME hud_feedback_systems COMMAND gdp_feedback_tests)
    set_tests_properties(hud_feedback_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_pause_tests tests/test_pause.cpp)
    target_link_libraries(gdp_pause_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_pause_tests)
    gdp_enable_sanitizers(gdp_pause_tests)

    add_test(NAME pause_menu_systems COMMAND gdp_pause_tests)
    set_tests_properties(pause_menu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_main_menu_tests tests/test_main_menu.cpp)
    target_link_libraries(gdp_main_menu_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_main_menu_tests)
    gdp_enable_sanitizers(gdp_main_menu_tests)

    add_test(NAME main_menu_systems COMMAND gdp_main_menu_tests)
    set_tests_properties(main_menu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_progression_tests tests/test_progression.cpp)
    target_link_libraries(gdp_progression_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_progression_tests)
    gdp_enable_sanitizers(gdp_progression_tests)

    add_test(NAME progression_systems COMMAND gdp_progression_tests)
    set_tests_properties(progression_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_session_tests tests/test_session.cpp)
    target_link_libraries(gdp_session_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_session_tests)
    gdp_enable_sanitizers(gdp_session_tests)

    add_test(NAME session_systems COMMAND gdp_session_tests)
    set_tests_properties(session_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_room_traversal_tests tests/test_room_traversal.cpp)
    target_link_libraries(gdp_room_traversal_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_room_traversal_tests)
    gdp_enable_sanitizers(gdp_room_traversal_tests)

    add_test(NAME room_traversal_systems COMMAND gdp_room_traversal_tests)
    set_tests_properties(room_traversal_systems PROPERTIES TIMEOUT 45)

    add_executable(gdp_loot_tests tests/test_loot.cpp)
    target_link_libraries(gdp_loot_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_loot_tests)
    gdp_enable_sanitizers(gdp_loot_tests)

    add_test(NAME loot_systems COMMAND gdp_loot_tests)
    set_tests_properties(loot_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_prop_tests tests/test_props.cpp)
    target_link_libraries(gdp_prop_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_prop_tests)
    gdp_enable_sanitizers(gdp_prop_tests)

    add_test(NAME prop_systems COMMAND gdp_prop_tests)
    set_tests_properties(prop_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_event_tests tests/test_events.cpp)
    target_link_libraries(gdp_event_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_event_tests)
    gdp_enable_sanitizers(gdp_event_tests)

    add_test(NAME room_event_systems COMMAND gdp_event_tests)
    set_tests_properties(room_event_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_lighting_tests tests/test_lighting.cpp)
    target_link_libraries(gdp_lighting_tests PRIVATE gdp_world)
    gdp_enable_warnings(gdp_lighting_tests)
    gdp_enable_sanitizers(gdp_lighting_tests)

    add_test(NAME room_lighting_systems COMMAND gdp_lighting_tests)
    set_tests_properties(room_lighting_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_combat_tests tests/test_combat.cpp)
    target_link_libraries(gdp_combat_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_combat_tests)
    gdp_enable_sanitizers(gdp_combat_tests)

    add_test(NAME combat_systems COMMAND gdp_combat_tests)
    set_tests_properties(combat_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_entity_tests tests/test_entities.cpp)
    target_link_libraries(gdp_entity_tests PRIVATE gdp_entities)
    gdp_enable_warnings(gdp_entity_tests)
    gdp_enable_sanitizers(gdp_entity_tests)

    add_test(NAME entity_manager_systems COMMAND gdp_entity_tests)
    set_tests_properties(entity_manager_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_world_tests tests/test_world.cpp)
    target_link_libraries(gdp_world_tests PRIVATE gdp_world)
    gdp_enable_warnings(gdp_world_tests)
    gdp_enable_sanitizers(gdp_world_tests)

    add_test(NAME world_module_systems COMMAND gdp_world_tests)
    set_tests_properties(world_module_systems PROPERTIES TIMEOUT 10)

    add_test(NAME bootstrap_headless COMMAND gdp --headless --frames 3)
    set_tests_properties(bootstrap_headless PROPERTIES TIMEOUT 10)

    add_test(NAME seeded_world_headless COMMAND gdp --headless --frames 1 --seed 12345)
    set_tests_properties(seeded_world_headless PROPERTIES TIMEOUT 10)

    add_test(NAME first_gameplay_headless COMMAND gdp --headless --frames 900 --seed 12345)
    set_tests_properties(
        first_gameplay_headless
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Player died"
            TIMEOUT 10
    )

    add_test(NAME command_line_help COMMAND gdp --help)
    set_tests_properties(command_line_help PROPERTIES TIMEOUT 10)

    if(UNIX AND NOT APPLE)
        add_executable(gdp_renderer_gl_tests tests/test_renderer_gl.cpp)
        target_link_libraries(gdp_renderer_gl_tests PRIVATE gdp_renderer)
        gdp_enable_warnings(gdp_renderer_gl_tests)
        gdp_enable_sanitizers(gdp_renderer_gl_tests)

        add_test(NAME renderer_opengl_pixels COMMAND gdp_renderer_gl_tests)
        set_tests_properties(
            renderer_opengl_pixels
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
                TIMEOUT 10
        )

        add_test(NAME bootstrap_opengl_offscreen COMMAND gdp --frames 2)
        set_tests_properties(
            bootstrap_opengl_offscreen
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "Main menu initialized before world generation"
                TIMEOUT 10
        )

        add_test(
            NAME main_menu_defers_world_generation
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_main_menu_bootstrap.cmake
        )
        set_tests_properties(main_menu_defers_world_generation PROPERTIES TIMEOUT 10)

        add_test(NAME bootstrap_audio_device_fallback COMMAND gdp --frames 2)
        set_tests_properties(
            bootstrap_audio_device_fallback
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=gdp_missing"
                PASS_REGULAR_EXPRESSION "continuing silently"
                TIMEOUT 10
        )
    endif()
endif()
