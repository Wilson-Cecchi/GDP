cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 NEW)

project(
    GDP
    VERSION 0.1.0
    DESCRIPTION "GDP procedural retro-industrial FPS"
    LANGUAGES CXX
)

if(MSVC)
    # Keep the downloadable Windows package independent from a separately
    # installed Visual C++ runtime. This also applies to fetched dependencies.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
    wkc_forge_core
    STATIC
    src/core/Hash.cpp
    src/core/Log.cpp
    src/core/Random.cpp
    src/core/Seed.cpp
    src/core/Time.cpp
)

target_compile_features(wkc_forge_core PUBLIC cxx_std_20)
target_include_directories(wkc_forge_core PUBLIC src)
gdp_enable_warnings(wkc_forge_core)
gdp_enable_sanitizers(wkc_forge_core)

add_library(
    wkc_forge_ai
    STATIC
    src/ai/NeuralNetwork.cpp
    src/ai/Genome.cpp
)

target_compile_features(wkc_forge_ai PUBLIC cxx_std_20)
target_include_directories(wkc_forge_ai PUBLIC src)
target_link_libraries(wkc_forge_ai PUBLIC wkc_forge_core)
gdp_enable_warnings(wkc_forge_ai)
gdp_enable_sanitizers(wkc_forge_ai)

add_library(
    wkc_forge_platform
    STATIC
    src/platform/Input.cpp
)

target_compile_features(wkc_forge_platform PUBLIC cxx_std_20)
target_include_directories(wkc_forge_platform PUBLIC src)
target_link_libraries(wkc_forge_platform PUBLIC wkc_forge_core)
gdp_enable_warnings(wkc_forge_platform)
gdp_enable_sanitizers(wkc_forge_platform)

add_library(
    wkc_forge_platform_sdl
    STATIC
    src/platform/SdlInput.cpp
)

target_compile_features(wkc_forge_platform_sdl PUBLIC cxx_std_20)
target_include_directories(wkc_forge_platform_sdl PUBLIC src)
target_link_libraries(wkc_forge_platform_sdl PUBLIC wkc_forge_platform SDL3::SDL3)
gdp_enable_warnings(wkc_forge_platform_sdl)
gdp_enable_sanitizers(wkc_forge_platform_sdl)

add_library(
    gdp_entities
    STATIC
    src/entities/EntityManager.cpp
)

target_compile_features(gdp_entities PUBLIC cxx_std_20)
target_include_directories(gdp_entities PUBLIC src)
target_link_libraries(gdp_entities PUBLIC wkc_forge_core)
gdp_enable_warnings(gdp_entities)
gdp_enable_sanitizers(gdp_entities)

add_library(
    wkc_forge_audio
    STATIC
    src/audio/AudioFoundation.cpp
    src/audio/AudioMixer.cpp
)

target_compile_features(wkc_forge_audio PUBLIC cxx_std_20)
target_include_directories(wkc_forge_audio PUBLIC src)
target_link_libraries(wkc_forge_audio PUBLIC wkc_forge_core SDL3::SDL3)
gdp_enable_warnings(wkc_forge_audio)
gdp_enable_sanitizers(wkc_forge_audio)

add_library(
    gdp_audio
    STATIC
    src/game/audio/AudioRecipes.cpp
    src/game/audio/AudioSystem.cpp
)

target_compile_features(gdp_audio PUBLIC cxx_std_20)
target_include_directories(gdp_audio PUBLIC src)
target_link_libraries(gdp_audio PUBLIC wkc_forge_audio wkc_forge_core)
gdp_enable_warnings(gdp_audio)
gdp_enable_sanitizers(gdp_audio)

add_library(
    gdp_gameplay
    STATIC
    src/gameplay/Campaign.cpp
    src/gameplay/Collision.cpp
    src/gameplay/Combat.cpp
    src/gameplay/Door.cpp
    src/gameplay/Enemy.cpp
    src/gameplay/EnemySensors.cpp
    src/gameplay/ExitConfirmation.cpp
    src/gameplay/Feedback.cpp
    src/gameplay/Footstep.cpp
    src/gameplay/Loot.cpp
    src/gameplay/MainMenu.cpp
    src/gameplay/Pause.cpp
    src/gameplay/Player.cpp
    src/gameplay/Progression.cpp
    src/gameplay/Props.cpp
    src/gameplay/ResultMenu.cpp
    src/gameplay/RoomEvents.cpp
    src/gameplay/RoomTraversal.cpp
    src/gameplay/Session.cpp
    src/gameplay/SettingsMenu.cpp
    src/gameplay/Speedrun.cpp
    src/gameplay/ViewMotion.cpp
    src/gameplay/Weapon.cpp
)

target_compile_features(gdp_gameplay PUBLIC cxx_std_20)
target_include_directories(gdp_gameplay PUBLIC src)
target_link_libraries(gdp_gameplay PUBLIC wkc_forge_core wkc_forge_ai gdp_entities)
gdp_enable_warnings(gdp_gameplay)
gdp_enable_sanitizers(gdp_gameplay)

add_library(
    wkc_forge_render
    STATIC
    src/render/Camera.cpp
    src/render/Framebuffer.cpp
    src/render/GlApi.cpp
    src/render/GpuTimer.cpp
    src/render/Mesh.cpp
    src/render/RasterState.cpp
    src/render/Shader.cpp
    src/render/Texture.cpp
)

target_compile_features(wkc_forge_render PUBLIC cxx_std_20)
target_include_directories(wkc_forge_render PUBLIC src)
target_link_libraries(wkc_forge_render PUBLIC wkc_forge_core SDL3::SDL3 OpenGL::GL)
gdp_enable_warnings(wkc_forge_render)
gdp_enable_sanitizers(wkc_forge_render)

add_library(wkc_forge_resources INTERFACE)
target_compile_features(wkc_forge_resources INTERFACE cxx_std_20)
target_include_directories(wkc_forge_resources INTERFACE src)

add_library(
    wkc_forge_procedural
    STATIC
    src/procedural/AssetRecipe.cpp
    src/procedural/MeshGenerator.cpp
    src/procedural/TextureGenerator.cpp
)

target_compile_features(wkc_forge_procedural PUBLIC cxx_std_20)
target_include_directories(wkc_forge_procedural PUBLIC src)
target_link_libraries(wkc_forge_procedural PUBLIC wkc_forge_core wkc_forge_render)
gdp_enable_warnings(wkc_forge_procedural)
gdp_enable_sanitizers(wkc_forge_procedural)

add_library(
    gdp_visual_content
    STATIC
    src/game/visual/EnemyGenerator.cpp
    src/game/visual/RoomMaterialGenerator.cpp
    src/game/visual/VisualAssets.cpp
)

target_compile_features(gdp_visual_content PUBLIC cxx_std_20)
target_include_directories(gdp_visual_content PUBLIC src)
target_link_libraries(
    gdp_visual_content
    PUBLIC wkc_forge_core wkc_forge_procedural wkc_forge_render
)
gdp_enable_warnings(gdp_visual_content)
gdp_enable_sanitizers(gdp_visual_content)

add_library(
    gdp_visual_renderer
    STATIC
    src/game/visual/GdpRenderData.cpp
    src/game/visual/GdpRenderer.cpp
    src/game/visual/WeaponViewmodelAssets.cpp
)

target_compile_features(gdp_visual_renderer PUBLIC cxx_std_20)
target_include_directories(gdp_visual_renderer PUBLIC src)
target_link_libraries(
    gdp_visual_renderer
    PUBLIC
    wkc_forge_core
    wkc_forge_procedural
    wkc_forge_render
    wkc_forge_resources
    gdp_visual_content
)
gdp_enable_warnings(gdp_visual_renderer)
gdp_enable_sanitizers(gdp_visual_renderer)

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
target_link_libraries(gdp_world PUBLIC wkc_forge_core gdp_entities)
gdp_enable_warnings(gdp_world)
gdp_enable_sanitizers(gdp_world)

target_link_libraries(gdp_gameplay PUBLIC gdp_world)

add_library(
    gdp_persistence
    STATIC
    src/persistence/CampaignCheckpointStore.cpp
    src/persistence/SettingsStore.cpp
    src/persistence/SpeedrunRecordStore.cpp
    src/persistence/WorkerBrainStore.cpp
)

target_compile_features(gdp_persistence PUBLIC cxx_std_20)
target_include_directories(gdp_persistence PUBLIC src)
target_link_libraries(gdp_persistence PUBLIC gdp_gameplay)
gdp_enable_warnings(gdp_persistence)
gdp_enable_sanitizers(gdp_persistence)

add_library(
    gdp_training
    STATIC
    src/game/ai/WorkerTraining.cpp
)
target_compile_features(gdp_training PUBLIC cxx_std_20)
target_include_directories(gdp_training PUBLIC src)
target_link_libraries(gdp_training PUBLIC gdp_gameplay gdp_persistence)
gdp_enable_warnings(gdp_training)
gdp_enable_sanitizers(gdp_training)

add_executable(
    gdp
    src/main.cpp
    src/game/Application.cpp
    src/game/LaunchSeed.cpp
    src/game/PerformanceMonitor.cpp
)

target_link_libraries(
    gdp
    PRIVATE
    gdp_audio
    wkc_forge_core
    gdp_entities
    gdp_gameplay
    gdp_persistence
    gdp_training
    wkc_forge_platform
    wkc_forge_platform_sdl
    gdp_visual_renderer
    gdp_world
    SDL3::SDL3
)
gdp_enable_warnings(gdp)
gdp_enable_sanitizers(gdp)

add_custom_target(
    graphics-benchmark
    COMMAND ${CMAKE_COMMAND}
        -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_graphics_benchmark.cmake
    DEPENDS gdp
    USES_TERMINAL
    COMMENT "Benchmarking all GDP graphics profiles"
)

add_custom_target(
    graphics-benchmark-stable
    COMMAND ${CMAKE_COMMAND}
        -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
        -DGDP_REPEATS=3
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_graphics_benchmark.cmake
    DEPENDS gdp
    USES_TERMINAL
    COMMENT "Benchmarking all GDP graphics profiles three times"
)

if(WIN32)
    install(TARGETS gdp RUNTIME DESTINATION .)

    set(GDP_SDL_SHARED_TARGET "")
    if(TARGET SDL3::SDL3-shared)
        set(GDP_SDL_SHARED_TARGET SDL3::SDL3-shared)
    elseif(TARGET SDL3-shared)
        set(GDP_SDL_SHARED_TARGET SDL3-shared)
    endif()

    if(GDP_SDL_SHARED_TARGET)
        add_custom_command(
            TARGET gdp
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${GDP_SDL_SHARED_TARGET}>"
                "$<TARGET_FILE_DIR:gdp>"
            COMMENT "Copying the SDL3 runtime beside gdp.exe"
            VERBATIM
        )
        install(
            FILES "$<TARGET_FILE:${GDP_SDL_SHARED_TARGET}>"
            DESTINATION .
        )
    endif()

    install(FILES README_WINDOWS.md DESTINATION .)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(GDP_WINDOWS_ARCHITECTURE x64)
    else()
        set(GDP_WINDOWS_ARCHITECTURE x86)
    endif()
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_FILE_NAME "GDP-Windows-${GDP_WINDOWS_ARCHITECTURE}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
endif()

if(GDP_BUILD_TESTS)
    include(CTest)

    add_test(
        NAME architecture_boundaries
        COMMAND ${CMAKE_COMMAND}
            -DGDP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_architecture_boundaries.cmake
    )
    set_tests_properties(architecture_boundaries PROPERTIES TIMEOUT 10)

    add_executable(wkc_forge_core_tests tests/test_core.cpp)
    target_link_libraries(wkc_forge_core_tests PRIVATE wkc_forge_core)
    gdp_enable_warnings(wkc_forge_core_tests)
    gdp_enable_sanitizers(wkc_forge_core_tests)

    add_test(NAME core_systems COMMAND wkc_forge_core_tests)
    set_tests_properties(core_systems PROPERTIES TIMEOUT 10)

    add_executable(wkc_forge_ai_tests tests/test_ai.cpp)
    target_link_libraries(wkc_forge_ai_tests PRIVATE wkc_forge_ai)
    gdp_enable_warnings(wkc_forge_ai_tests)
    gdp_enable_sanitizers(wkc_forge_ai_tests)

    add_test(NAME ai_foundation_systems COMMAND wkc_forge_ai_tests)
    set_tests_properties(ai_foundation_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_worker_training_tests tests/test_worker_training.cpp)
    target_link_libraries(gdp_worker_training_tests PRIVATE gdp_training)
    gdp_enable_warnings(gdp_worker_training_tests)
    gdp_enable_sanitizers(gdp_worker_training_tests)

    add_test(NAME worker_training_systems COMMAND gdp_worker_training_tests)
    set_tests_properties(worker_training_systems PROPERTIES TIMEOUT 120)

    add_test(
        NAME worker_training_cli_smoke
        COMMAND gdp --ai-training worker --population 4 --generations 2 --seed 42
    )
    set_tests_properties(worker_training_cli_smoke PROPERTIES TIMEOUT 30)

    add_test(
        NAME all_enemy_training_cli_smoke
        COMMAND gdp --ai-training all --population 2 --generations 1 --seed 42
    )
    set_tests_properties(all_enemy_training_cli_smoke PROPERTIES TIMEOUT 30)

    add_executable(gdp_enemy_sensors_tests tests/test_enemy_sensors.cpp)
    target_link_libraries(gdp_enemy_sensors_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_enemy_sensors_tests)
    gdp_enable_sanitizers(gdp_enemy_sensors_tests)

    add_test(NAME enemy_sensors_systems COMMAND gdp_enemy_sensors_tests)
    set_tests_properties(enemy_sensors_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_seed_domain_tests tests/test_seed_domains.cpp)
    target_link_libraries(gdp_seed_domain_tests PRIVATE wkc_forge_core)
    gdp_enable_warnings(gdp_seed_domain_tests)
    gdp_enable_sanitizers(gdp_seed_domain_tests)

    add_test(NAME gdp_seed_domain_systems COMMAND gdp_seed_domain_tests)
    set_tests_properties(gdp_seed_domain_systems PROPERTIES TIMEOUT 10)

    add_executable(
        gdp_launch_seed_tests
        tests/test_launch_seed.cpp
        src/game/LaunchSeed.cpp
    )
    target_link_libraries(gdp_launch_seed_tests PRIVATE wkc_forge_core gdp_world)
    gdp_enable_warnings(gdp_launch_seed_tests)
    gdp_enable_sanitizers(gdp_launch_seed_tests)

    add_test(NAME launch_seed_systems COMMAND gdp_launch_seed_tests)
    set_tests_properties(launch_seed_systems PROPERTIES TIMEOUT 10)

    add_executable(
        gdp_performance_tests
        tests/test_performance.cpp
        src/game/PerformanceMonitor.cpp
    )
    target_link_libraries(gdp_performance_tests PRIVATE wkc_forge_core)
    gdp_enable_warnings(gdp_performance_tests)
    gdp_enable_sanitizers(gdp_performance_tests)

    add_test(NAME performance_monitor_systems COMMAND gdp_performance_tests)
    set_tests_properties(performance_monitor_systems PROPERTIES TIMEOUT 10)

    add_executable(wkc_forge_platform_tests tests/test_input.cpp)
    target_link_libraries(wkc_forge_platform_tests PRIVATE wkc_forge_platform)
    gdp_enable_warnings(wkc_forge_platform_tests)
    gdp_enable_sanitizers(wkc_forge_platform_tests)

    add_test(NAME platform_input_systems COMMAND wkc_forge_platform_tests)
    set_tests_properties(platform_input_systems PROPERTIES TIMEOUT 10)

    add_executable(wkc_forge_platform_sdl_tests tests/test_sdl_input.cpp)
    target_link_libraries(wkc_forge_platform_sdl_tests PRIVATE wkc_forge_platform_sdl)
    gdp_enable_warnings(wkc_forge_platform_sdl_tests)
    gdp_enable_sanitizers(wkc_forge_platform_sdl_tests)

    add_test(NAME sdl_input_adapter_systems COMMAND wkc_forge_platform_sdl_tests)
    set_tests_properties(sdl_input_adapter_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_audio_tests tests/test_audio.cpp)
    target_link_libraries(gdp_audio_tests PRIVATE gdp_audio)
    gdp_enable_warnings(gdp_audio_tests)
    gdp_enable_sanitizers(gdp_audio_tests)

    add_test(NAME procedural_audio_systems COMMAND gdp_audio_tests)
    set_tests_properties(procedural_audio_systems PROPERTIES TIMEOUT 15)

    add_executable(wkc_forge_audio_tests tests/test_audio_foundation.cpp)
    target_link_libraries(wkc_forge_audio_tests PRIVATE wkc_forge_audio)
    gdp_enable_warnings(wkc_forge_audio_tests)
    gdp_enable_sanitizers(wkc_forge_audio_tests)

    add_test(NAME audio_foundation_systems COMMAND wkc_forge_audio_tests)
    set_tests_properties(audio_foundation_systems PROPERTIES TIMEOUT 10)

    add_executable(wkc_forge_resources_tests tests/test_resources.cpp)
    target_link_libraries(wkc_forge_resources_tests PRIVATE wkc_forge_resources)
    gdp_enable_warnings(wkc_forge_resources_tests)
    gdp_enable_sanitizers(wkc_forge_resources_tests)

    add_test(NAME resource_cache_systems COMMAND wkc_forge_resources_tests)
    set_tests_properties(resource_cache_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_renderer_tests tests/test_renderer.cpp)
    target_link_libraries(gdp_renderer_tests PRIVATE gdp_visual_renderer)
    gdp_enable_warnings(gdp_renderer_tests)
    gdp_enable_sanitizers(gdp_renderer_tests)

    add_test(NAME renderer_cpu_systems COMMAND gdp_renderer_tests)
    set_tests_properties(renderer_cpu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_asset_tests tests/test_assets.cpp)
    target_link_libraries(
        gdp_asset_tests
        PRIVATE wkc_forge_procedural wkc_forge_resources gdp_visual_content
    )
    gdp_enable_warnings(gdp_asset_tests)
    gdp_enable_sanitizers(gdp_asset_tests)

    add_test(NAME procedural_asset_systems COMMAND gdp_asset_tests)
    set_tests_properties(procedural_asset_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_enemy_generation_tests tests/test_enemy_generation.cpp)
    target_link_libraries(gdp_enemy_generation_tests PRIVATE gdp_visual_content)
    gdp_enable_warnings(gdp_enemy_generation_tests)
    gdp_enable_sanitizers(gdp_enemy_generation_tests)

    add_test(NAME enemy_generation_systems COMMAND gdp_enemy_generation_tests)
    set_tests_properties(enemy_generation_systems PROPERTIES TIMEOUT 60)

    add_executable(gdp_room_material_tests tests/test_room_materials.cpp)
    target_link_libraries(gdp_room_material_tests PRIVATE gdp_visual_content)
    gdp_enable_warnings(gdp_room_material_tests)
    gdp_enable_sanitizers(gdp_room_material_tests)

    add_test(NAME room_material_systems COMMAND gdp_room_material_tests)
    set_tests_properties(room_material_systems PROPERTIES TIMEOUT 300)

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

    add_executable(gdp_result_menu_tests tests/test_result_menu.cpp)
    target_link_libraries(gdp_result_menu_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_result_menu_tests)
    gdp_enable_sanitizers(gdp_result_menu_tests)

    add_test(NAME result_menu_systems COMMAND gdp_result_menu_tests)
    set_tests_properties(result_menu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_exit_confirmation_tests tests/test_exit_confirmation.cpp)
    target_link_libraries(gdp_exit_confirmation_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_exit_confirmation_tests)
    gdp_enable_sanitizers(gdp_exit_confirmation_tests)

    add_test(NAME exit_confirmation_systems COMMAND gdp_exit_confirmation_tests)
    set_tests_properties(exit_confirmation_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_settings_menu_tests tests/test_settings_menu.cpp)
    target_link_libraries(gdp_settings_menu_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_settings_menu_tests)
    gdp_enable_sanitizers(gdp_settings_menu_tests)

    add_test(NAME settings_menu_systems COMMAND gdp_settings_menu_tests)
    set_tests_properties(settings_menu_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_settings_store_tests tests/test_settings_store.cpp)
    target_link_libraries(gdp_settings_store_tests PRIVATE gdp_persistence)
    gdp_enable_warnings(gdp_settings_store_tests)
    gdp_enable_sanitizers(gdp_settings_store_tests)

    add_test(NAME settings_persistence_systems COMMAND gdp_settings_store_tests)
    set_tests_properties(settings_persistence_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_progression_tests tests/test_progression.cpp)
    target_link_libraries(gdp_progression_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_progression_tests)
    gdp_enable_sanitizers(gdp_progression_tests)

    add_test(NAME progression_systems COMMAND gdp_progression_tests)
    set_tests_properties(progression_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_campaign_tests tests/test_campaign.cpp)
    target_link_libraries(gdp_campaign_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_campaign_tests)
    gdp_enable_sanitizers(gdp_campaign_tests)

    add_test(NAME campaign_systems COMMAND gdp_campaign_tests)
    set_tests_properties(campaign_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_session_tests tests/test_session.cpp)
    target_link_libraries(gdp_session_tests PRIVATE gdp_gameplay gdp_world)
    gdp_enable_warnings(gdp_session_tests)
    gdp_enable_sanitizers(gdp_session_tests)

    add_test(NAME session_systems COMMAND gdp_session_tests)
    set_tests_properties(session_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_speedrun_tests tests/test_speedrun.cpp)
    target_link_libraries(gdp_speedrun_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_speedrun_tests)
    gdp_enable_sanitizers(gdp_speedrun_tests)

    add_test(NAME speedrun_systems COMMAND gdp_speedrun_tests)
    set_tests_properties(speedrun_systems PROPERTIES TIMEOUT 10)

    add_executable(
        gdp_speedrun_record_store_tests
        tests/test_speedrun_record_store.cpp
    )
    target_link_libraries(gdp_speedrun_record_store_tests PRIVATE gdp_persistence)
    gdp_enable_warnings(gdp_speedrun_record_store_tests)
    gdp_enable_sanitizers(gdp_speedrun_record_store_tests)

    add_test(
        NAME speedrun_record_persistence_systems
        COMMAND gdp_speedrun_record_store_tests
    )
    set_tests_properties(speedrun_record_persistence_systems PROPERTIES TIMEOUT 10)

    add_executable(
        gdp_campaign_checkpoint_store_tests
        tests/test_campaign_checkpoint_store.cpp
    )
    target_link_libraries(gdp_campaign_checkpoint_store_tests PRIVATE gdp_persistence)
    gdp_enable_warnings(gdp_campaign_checkpoint_store_tests)
    gdp_enable_sanitizers(gdp_campaign_checkpoint_store_tests)

    add_test(
        NAME campaign_checkpoint_persistence_systems
        COMMAND gdp_campaign_checkpoint_store_tests
    )
    set_tests_properties(
        campaign_checkpoint_persistence_systems PROPERTIES TIMEOUT 10)

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
        add_executable(gdp_capture_industrial_wall tests/capture_industrial_wall.cpp)
        target_link_libraries(gdp_capture_industrial_wall PRIVATE gdp_visual_renderer)
        gdp_enable_warnings(gdp_capture_industrial_wall)
        gdp_enable_sanitizers(gdp_capture_industrial_wall)

        add_executable(gdp_renderer_gl_tests tests/test_renderer_gl.cpp)
        target_link_libraries(gdp_renderer_gl_tests PRIVATE gdp_visual_renderer)
        gdp_enable_warnings(gdp_renderer_gl_tests)
        gdp_enable_sanitizers(gdp_renderer_gl_tests)

        add_executable(
            wkc_forge_instancing_gl_tests
            tests/test_instancing_gl.cpp
        )
        target_link_libraries(
            wkc_forge_instancing_gl_tests PRIVATE wkc_forge_render)
        gdp_enable_warnings(wkc_forge_instancing_gl_tests)
        gdp_enable_sanitizers(wkc_forge_instancing_gl_tests)

        add_test(NAME renderer_opengl_pixels COMMAND gdp_renderer_gl_tests)
        set_tests_properties(
            renderer_opengl_pixels
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
                TIMEOUT 20
        )

        add_test(
            NAME renderer_instancing_opengl
            COMMAND wkc_forge_instancing_gl_tests)
        set_tests_properties(
            renderer_instancing_opengl
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
                TIMEOUT 20
        )

        add_test(NAME bootstrap_opengl_offscreen COMMAND gdp --frames 2)
        set_tests_properties(
            bootstrap_opengl_offscreen
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "Main menu initialized before world generation"
                TIMEOUT 20
        )

        add_test(
            NAME graphics_benchmark_smoke
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -DGDP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_graphics_benchmark.cmake)
        set_tests_properties(
            graphics_benchmark_smoke
            PROPERTIES
                TIMEOUT 180
        )

        add_test(
            NAME main_menu_defers_world_generation
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_main_menu_bootstrap.cmake
        )
        set_tests_properties(main_menu_defers_world_generation PROPERTIES TIMEOUT 20)

        add_test(NAME bootstrap_audio_device_fallback COMMAND gdp --frames 2)
        set_tests_properties(
            bootstrap_audio_device_fallback
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=gdp_missing"
                PASS_REGULAR_EXPRESSION "continuing silently"
                TIMEOUT 20
        )

        add_test(
            NAME fullscreen_setting_bootstrap
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -DGDP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fullscreen_setting.cmake
        )
        set_tests_properties(fullscreen_setting_bootstrap PROPERTIES TIMEOUT 30)

        add_test(
            NAME window_resolution_setting_bootstrap
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -DGDP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_window_resolution.cmake
        )
        set_tests_properties(
            window_resolution_setting_bootstrap PROPERTIES TIMEOUT 30)

        add_test(
            NAME vsync_setting_bootstrap
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -DGDP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vsync_setting.cmake
        )
        set_tests_properties(vsync_setting_bootstrap PROPERTIES TIMEOUT 30)

        add_test(
            NAME graphics_quality_setting_bootstrap
            COMMAND ${CMAKE_COMMAND}
                -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
                -DGDP_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_graphics_quality.cmake
        )
        set_tests_properties(
            graphics_quality_setting_bootstrap PROPERTIES TIMEOUT 30)
    endif()
endif()
