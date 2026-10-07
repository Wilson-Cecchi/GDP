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
option(GDP_ENABLE_TEST_MODE "Include the dedicated development test arena (disable for shipping)" ON)
option(GDP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(GDP_PORTABLE_PACKAGE "Build a self-contained Linux or macOS download" OFF)

if(GDP_PORTABLE_PACKAGE AND WIN32)
    message(FATAL_ERROR "GDP_PORTABLE_PACKAGE is for Linux and macOS; use windows-release on Windows")
endif()

if(NOT GDP_PORTABLE_PACKAGE)
    find_package(SDL3 3.4 CONFIG QUIET)
endif()

if(GDP_PORTABLE_PACKAGE OR NOT SDL3_FOUND)
    if(NOT GDP_FETCH_SDL3)
        message(FATAL_ERROR "SDL3 was not found and GDP_FETCH_SDL3 is OFF")
    endif()

    if(GDP_PORTABLE_PACKAGE)
        set(SDL_SHARED OFF CACHE BOOL "Do not need a separate SDL3 runtime" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Link SDL3 into the portable executable" FORCE)
    else()
        set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "Do not build SDL3 as a static library" FORCE)
    endif()
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
    if(GDP_PORTABLE_PACKAGE)
        FetchContent_GetProperties(SDL3 SOURCE_DIR GDP_SDL_SOURCE_DIR)
    endif()
endif()

find_package(OpenGL REQUIRED)

function(gdp_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
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
    src/platform/SdlGamepad.cpp
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
    src/gameplay/ControlBindings.cpp
    src/gameplay/Collision.cpp
    src/gameplay/Combat.cpp
    src/gameplay/Door.cpp
    src/gameplay/Enemy.cpp
    src/gameplay/EnemyProjectile.cpp
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
target_link_libraries(gdp_gameplay PUBLIC wkc_forge_core wkc_forge_ai gdp_entities wkc_forge_platform)
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
    src/game/visual/MenuBackdrop.cpp
    src/game/Localization.cpp
    src/game/visual/WeaponViewmodelAssets.cpp
    src/game/visual/PlayerBodyAssets.cpp
    src/game/visual/PlayerHandsAssets.cpp
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
    src/persistence/SpeedrunLeaderboardStore.cpp
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
    src/game/ai/ActiveEnemyBrains.cpp
    src/game/ai/WorkerTraining.cpp
    src/game/ai/TrainingReward.cpp
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
    src/game/ApplicationMenus.cpp
    src/game/MenuFlow.cpp
    src/game/HudBoss.cpp
    src/game/HudMap.cpp
    src/game/LaunchSeed.cpp
    src/game/PerformanceMonitor.cpp
)
target_compile_definitions(gdp PRIVATE GDP_ENABLE_TEST_MODE=$<BOOL:${GDP_ENABLE_TEST_MODE}>)
if(GDP_ENABLE_TEST_MODE)
    target_sources(gdp PRIVATE src/game/TestArena.cpp)
endif()

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
if(APPLE)
    set_target_properties(gdp PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME IronSeed
        MACOSX_BUNDLE_BUNDLE_NAME "Iron Seed"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.wkc.ironseed"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

add_executable(gdp_ai_gameplay_eval tools/ai_gameplay_eval.cpp)
target_link_libraries(gdp_ai_gameplay_eval PRIVATE gdp_training gdp_world)
gdp_enable_warnings(gdp_ai_gameplay_eval)
gdp_enable_sanitizers(gdp_ai_gameplay_eval)

set(GDP_ACTIVE_BRAIN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/worker.brain"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/guard.brain"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/officer.brain"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/engineer.brain"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/robot.brain"
)
if(APPLE)
    # SDL_GetBasePath() resolves to Contents/Resources for a macOS .app.
    set(GDP_BRAIN_OUTPUT_DIRECTORY "$<TARGET_BUNDLE_CONTENT_DIR:gdp>/Resources/ai")
else()
    set(GDP_BRAIN_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:gdp>/ai")
endif()
add_custom_target(gdp_brain_assets
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GDP_BRAIN_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GDP_ACTIVE_BRAIN_FILES} "${GDP_BRAIN_OUTPUT_DIRECTORY}"
    DEPENDS ${GDP_ACTIVE_BRAIN_FILES}
    VERBATIM
)
add_dependencies(gdp gdp_brain_assets)

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
    install(DIRECTORY assets/ai/ DESTINATION ai FILES_MATCHING PATTERN "*.brain")

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
    install(FILES assets/fonts/LICENSE-DejaVu.txt DESTINATION licenses)

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

if(UNIX AND NOT APPLE AND GDP_PORTABLE_PACKAGE)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        message(FATAL_ERROR "The Linux download workflow currently supports x64 only")
    endif()
    install(TARGETS gdp RUNTIME DESTINATION .)
    install(DIRECTORY assets/ai/ DESTINATION ai FILES_MATCHING PATTERN "*.brain")
    install(FILES README_LINUX.md DESTINATION .)
    install(FILES assets/fonts/LICENSE-DejaVu.txt DESTINATION licenses)
    install(FILES "${GDP_SDL_SOURCE_DIR}/LICENSE.txt"
        DESTINATION licenses RENAME LICENSE-SDL3.txt)
    set(CPACK_GENERATOR TGZ)
    set(CPACK_PACKAGE_FILE_NAME "IronSeed-Linux-x64")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
endif()

if(APPLE AND GDP_PORTABLE_PACKAGE)
    install(TARGETS gdp BUNDLE DESTINATION .)
    install(FILES README_MACOS.md DESTINATION .)
    install(FILES assets/fonts/LICENSE-DejaVu.txt
        DESTINATION IronSeed.app/Contents/Resources/licenses)
    install(FILES "${GDP_SDL_SOURCE_DIR}/LICENSE.txt"
        DESTINATION IronSeed.app/Contents/Resources/licenses
        RENAME LICENSE-SDL3.txt)
endif()

if(GDP_BUILD_TESTS)
    add_executable(gdp_frontend_tests tests/test_frontend.cpp src/game/MenuFlow.cpp)
    target_link_libraries(gdp_frontend_tests PRIVATE gdp_gameplay gdp_persistence gdp_visual_renderer)
    gdp_enable_warnings(gdp_frontend_tests)
    gdp_enable_sanitizers(gdp_frontend_tests)
    add_test(NAME frontend_menu_systems COMMAND gdp_frontend_tests)
    if(UNIX AND NOT APPLE AND GDP_ENABLE_TEST_MODE)
        get_target_property(GDP_APPLICATION_TEST_SOURCES gdp SOURCES)
        list(REMOVE_ITEM GDP_APPLICATION_TEST_SOURCES src/main.cpp)
        get_target_property(GDP_APPLICATION_TEST_LIBRARIES gdp LINK_LIBRARIES)
        add_executable(gdp_application_menu_tests tests/test_application_menus.cpp ${GDP_APPLICATION_TEST_SOURCES})
        target_compile_definitions(gdp_application_menu_tests PRIVATE GDP_ENABLE_TEST_MODE=1)
        target_link_libraries(gdp_application_menu_tests PRIVATE ${GDP_APPLICATION_TEST_LIBRARIES})
        gdp_enable_warnings(gdp_application_menu_tests)
        gdp_enable_sanitizers(gdp_application_menu_tests)
        add_dependencies(gdp_application_menu_tests gdp_brain_assets)
        add_test(NAME application_menu_integration COMMAND gdp_application_menu_tests)
        set_tests_properties(application_menu_integration PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy" TIMEOUT 120)
        if(GDP_ENABLE_SANITIZERS)
            # This fixture rebuilds many real procedural/GPU assets, including save migration.
            set_tests_properties(application_menu_integration PROPERTIES TIMEOUT 300)
        endif()
    endif()
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
    # ASan/UBSan runs the role-specific arena curriculum substantially slower.
    # The deterministic multi-archetype simulation is intentionally allowed
    # to run under a CPU quota on older laptops without false timeout failures.
    set_tests_properties(worker_training_systems PROPERTIES TIMEOUT 600)

    add_executable(gdp_active_enemy_brain_tests tests/test_active_enemy_brains.cpp)
    target_link_libraries(gdp_active_enemy_brain_tests PRIVATE gdp_training)
    target_compile_definitions(gdp_active_enemy_brain_tests PRIVATE
        GDP_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")
    gdp_enable_warnings(gdp_active_enemy_brain_tests)
    gdp_enable_sanitizers(gdp_active_enemy_brain_tests)
    add_test(NAME active_enemy_brain_catalog COMMAND gdp_active_enemy_brain_tests)
    set_tests_properties(active_enemy_brain_catalog PROPERTIES TIMEOUT 10)

    add_test(NAME ai_gameplay_eval_deterministic
        COMMAND ${CMAKE_COMMAND}
            -DGDP_AI_EVAL=$<TARGET_FILE:gdp_ai_gameplay_eval>
            -DGDP_BRAIN_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets/ai
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_ai_gameplay_eval.cmake)
    set_tests_properties(ai_gameplay_eval_deterministic PROPERTIES TIMEOUT 30)

    find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME ai_auto_promotion_policy
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
                -p test_ai_auto_promote.py)
        set_tests_properties(ai_auto_promotion_policy PROPERTIES
            TIMEOUT 15
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()

    add_test(NAME active_enemy_brains_headless
        COMMAND ${CMAKE_COMMAND}
            -DGDP_EXECUTABLE=$<TARGET_FILE:gdp>
            -DGDP_BRAIN_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets/ai
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_active_brains.cmake)
    set_tests_properties(active_enemy_brains_headless PROPERTIES TIMEOUT 15)

    add_test(
        NAME worker_training_cli_smoke
        COMMAND gdp --ai-training worker --population 4 --generations 2 --seed 42
    )
    set_tests_properties(worker_training_cli_smoke PROPERTIES TIMEOUT 120)

    add_test(
        NAME all_enemy_training_cli_smoke
        COMMAND gdp --ai-training all --population 2 --generations 1 --seed 42
    )
    set_tests_properties(all_enemy_training_cli_smoke PROPERTIES TIMEOUT 120)

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
    set_tests_properties(combat_systems PROPERTIES TIMEOUT 30)

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

    add_executable(gdp_hud_map_tests tests/test_hud_map.cpp src/game/HudMap.cpp)
    target_link_libraries(gdp_hud_map_tests PRIVATE gdp_world)
    gdp_enable_warnings(gdp_hud_map_tests)
    gdp_enable_sanitizers(gdp_hud_map_tests)

    add_test(NAME hud_map_systems COMMAND gdp_hud_map_tests)
    set_tests_properties(hud_map_systems PROPERTIES TIMEOUT 10)

    add_executable(gdp_hud_boss_tests tests/test_hud_boss.cpp src/game/HudBoss.cpp)
    target_link_libraries(gdp_hud_boss_tests PRIVATE gdp_gameplay)
    gdp_enable_warnings(gdp_hud_boss_tests)
    gdp_enable_sanitizers(gdp_hud_boss_tests)

    add_test(NAME hud_boss_systems COMMAND gdp_hud_boss_tests)
    set_tests_properties(hud_boss_systems PROPERTIES TIMEOUT 10)

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

        add_executable(gdp_renderer_gl_tests tests/test_renderer_gl.cpp src/game/MenuFlow.cpp)
        target_link_libraries(gdp_renderer_gl_tests PRIVATE gdp_visual_renderer gdp_gameplay)
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
        add_test(NAME weapon_reload_opengl COMMAND gdp_renderer_gl_tests --reload-only)
        add_test(NAME player_hands_opengl COMMAND gdp_renderer_gl_tests --player-hands-only)
        set_tests_properties(player_hands_opengl PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen" TIMEOUT 30)
        set_tests_properties(weapon_reload_opengl PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen" TIMEOUT 30)
        if(GDP_ENABLE_SANITIZERS)
            set_tests_properties(weapon_reload_opengl PROPERTIES TIMEOUT 60)
            set_tests_properties(player_hands_opengl PROPERTIES TIMEOUT 60)
        endif()
        set_tests_properties(
            renderer_opengl_pixels
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
                TIMEOUT 60
        )
        if(GDP_ENABLE_SANITIZERS)
            set_tests_properties(renderer_opengl_pixels PROPERTIES TIMEOUT 60)
        endif()

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
