if(NOT DEFINED GDP_SOURCE_DIR)
    message(FATAL_ERROR "GDP_SOURCE_DIR was not provided")
endif()

file(READ "${GDP_SOURCE_DIR}/CMakeLists.txt" root_cmake_contents)
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_core[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Generic core must belong to WKC Forge")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_core[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP-prefixed core target must not return")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_platform[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Generic input platform must belong to WKC Forge")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_platform_sdl[ \t\r\n]+STATIC")
    message(FATAL_ERROR "SDL input adapter must belong to WKC Forge")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_platform(_sdl)?[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP-prefixed platform targets must not return")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_procedural[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Generic procedural foundation must belong to WKC Forge")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_procedural_assets[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP-prefixed procedural foundation target must not return")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\(wkc_forge_resources[ \t]+INTERFACE\\)")
    message(FATAL_ERROR "Generic resource cache must belong to WKC Forge")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_audio[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Generic audio foundation must belong to WKC Forge")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_visual_renderer[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP concrete renderer target must be named gdp_visual_renderer")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_renderer[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Ambiguous gdp_renderer target must not return")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_render[ \t\r\n]+STATIC")
    message(FATAL_ERROR "Generic render foundation must belong to WKC Forge")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_render_foundation[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP-prefixed render foundation target must not return")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_entities[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP entity catalog must remain owned by gdp_entities")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_entities[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP entity catalog must not move into WKC Forge")
endif()
if(NOT root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+gdp_world[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP world generation must remain owned by gdp_world")
endif()
if(root_cmake_contents MATCHES
        "add_library\\([ \t\r\n]+wkc_forge_world[ \t\r\n]+STATIC")
    message(FATAL_ERROR "GDP world generation must not move into WKC Forge")
endif()
foreach(wkc_target IN ITEMS
    wkc_forge_core
    wkc_forge_platform
    wkc_forge_platform_sdl
    wkc_forge_audio
    wkc_forge_render
    wkc_forge_procedural
)
    if(root_cmake_contents MATCHES
            "target_link_libraries\\(${wkc_target}[^\\)]*gdp_[A-Za-z0-9_]+")
        message(FATAL_ERROR
            "WKC Forge target must not depend on GDP target: ${wkc_target}"
        )
    endif()
endforeach()
foreach(wkc_test_target IN ITEMS
    wkc_forge_core_tests
    wkc_forge_platform_tests
    wkc_forge_platform_sdl_tests
)
    if(NOT root_cmake_contents MATCHES
            "add_executable\\(${wkc_test_target}[ \t]+tests/")
        message(FATAL_ERROR
            "WKC Forge foundation test must use WKC identity: ${wkc_test_target}"
        )
    endif()
endforeach()
if(root_cmake_contents MATCHES
        "add_executable\\(gdp_(core|input|sdl_input)_tests")
    message(FATAL_ERROR
        "Engine-only test executables must not retain a GDP identity"
    )
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(wkc_forge_render[ \t]+PUBLIC[ \t]+wkc_forge_core")
    message(FATAL_ERROR "WKC Forge render must depend on WKC Forge core")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(wkc_forge_platform[ \t]+PUBLIC[ \t]+wkc_forge_core")
    message(FATAL_ERROR "WKC Forge platform must depend on WKC Forge core")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(wkc_forge_platform_sdl[ \t]+PUBLIC[ \t]+wkc_forge_platform")
    message(FATAL_ERROR "SDL adapter must depend on the generic WKC Forge platform")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(wkc_forge_procedural[ \t]+PUBLIC[ \t]+wkc_forge_core[ \t]+wkc_forge_render")
    message(FATAL_ERROR "WKC Forge procedural must depend only on Forge foundations")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\([ \t\r\n]+gdp_visual_renderer[^\\)]*wkc_forge_resources")
    message(FATAL_ERROR "GDP visual renderer must declare its resource cache dependency")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(wkc_forge_audio[ \t]+PUBLIC[ \t]+wkc_forge_core[ \t]+SDL3::SDL3")
    message(FATAL_ERROR "WKC Forge audio must own its core and SDL dependencies")
endif()
if(NOT root_cmake_contents MATCHES
        "target_link_libraries\\(gdp_audio[ \t]+PUBLIC[ \t]+wkc_forge_audio")
    message(FATAL_ERROR "GDP audio must consume the WKC Forge audio foundation")
endif()
if(root_cmake_contents MATCHES
        "target_link_libraries\\(gdp_audio[^\\)]*SDL3::SDL3")
    message(FATAL_ERROR "GDP audio facade must not depend directly on SDL")
endif()

set(core_directory "${GDP_SOURCE_DIR}/src/core")
set(game_application "${GDP_SOURCE_DIR}/src/game/Application.hpp")

if(EXISTS "${core_directory}/Application.cpp"
    OR EXISTS "${core_directory}/Application.hpp")
    message(FATAL_ERROR "GDP Application must not be owned by src/core")
endif()

if(NOT EXISTS "${game_application}")
    message(FATAL_ERROR "GDP Application is missing from src/game")
endif()

set(platform_input "${GDP_SOURCE_DIR}/src/platform/Input.hpp")
file(READ "${platform_input}" platform_input_header)
if(platform_input_header MATCHES "SDL")
    message(FATAL_ERROR "platform/Input.hpp must not expose SDL types")
endif()
file(GLOB platform_sources
    "${GDP_SOURCE_DIR}/src/platform/*.cpp"
    "${GDP_SOURCE_DIR}/src/platform/*.hpp"
)
foreach(source IN LISTS platform_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+wkc::platform[ \t{]"
        OR contents MATCHES "namespace[ \t]+gdp::platform[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "WKC Forge platform API must use wkc::platform: ${relative_source}"
        )
    endif()
endforeach()

file(READ "${core_directory}/Seed.hpp" core_seed_header)
if(core_seed_header MATCHES "enum[ \t]+class[ \t]+SeedDomain")
    message(FATAL_ERROR "core/Seed.hpp must not own GDP seed domains")
endif()
foreach(migrated_core_source IN ITEMS
    "${core_directory}/Hash.cpp"
    "${core_directory}/Hash.hpp"
    "${core_directory}/Log.cpp"
    "${core_directory}/Log.hpp"
    "${core_directory}/Math.hpp"
    "${core_directory}/Random.cpp"
    "${core_directory}/Random.hpp"
    "${core_directory}/Seed.cpp"
    "${core_directory}/Seed.hpp"
    "${core_directory}/Time.cpp"
    "${core_directory}/Time.hpp"
)
    file(READ "${migrated_core_source}" migrated_core_contents)
    if(NOT migrated_core_contents MATCHES "namespace[ \t]+wkc::core[ \t{]"
        OR migrated_core_contents MATCHES "namespace[ \t]+gdp::core[ \t{]")
        file(RELATIVE_PATH relative_source
            "${GDP_SOURCE_DIR}" "${migrated_core_source}")
        message(FATAL_ERROR
            "Migrated WKC Forge Core API must use wkc::core: ${relative_source}"
        )
    endif()
endforeach()
if(NOT EXISTS "${GDP_SOURCE_DIR}/src/game/SeedDomains.hpp")
    message(FATAL_ERROR "GDP seed domain catalog is missing from src/game")
endif()

file(GLOB entity_sources
    "${GDP_SOURCE_DIR}/src/entities/*.cpp"
    "${GDP_SOURCE_DIR}/src/entities/*.hpp"
)
foreach(source IN LISTS entity_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+gdp::entities[ \t{]"
        OR contents MATCHES "namespace[ \t]+wkc::entities[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "GDP entity catalog must use gdp::entities: ${relative_source}"
        )
    endif()
endforeach()
file(READ "${GDP_SOURCE_DIR}/src/entities/EntityManager.hpp"
    entity_manager_header)
if(NOT entity_manager_header MATCHES "enum[ \t]+class[ \t]+EnemyArchetype"
    OR NOT entity_manager_header MATCHES "IndustrialRobot"
    OR NOT entity_manager_header MATCHES "enum[ \t]+class[ \t]+PropType")
    message(FATAL_ERROR
        "GDP entity catalog must retain its game-specific enemy and prop contracts"
    )
endif()

file(GLOB world_sources
    "${GDP_SOURCE_DIR}/src/world/*.cpp"
    "${GDP_SOURCE_DIR}/src/world/*.hpp"
)
foreach(source IN LISTS world_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+gdp::world[ \t{]"
        OR contents MATCHES "namespace[ \t]+wkc::world[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "GDP world generation must use gdp::world: ${relative_source}"
        )
    endif()
endforeach()
file(READ "${GDP_SOURCE_DIR}/src/world/RoomModule.hpp" room_module_header)
if(NOT room_module_header MATCHES "enum[ \t]+class[ \t]+RoomRole"
    OR NOT room_module_header MATCHES "Boss"
    OR NOT room_module_header MATCHES "enum[ \t]+class[ \t]+RoomTheme"
    OR NOT room_module_header MATCHES "Laboratory")
    message(FATAL_ERROR
        "GDP world must retain its game-specific room roles and themes"
    )
endif()

set(game_visual_assets "${GDP_SOURCE_DIR}/src/game/visual/VisualAssets.hpp")
if(NOT EXISTS "${game_visual_assets}")
    message(FATAL_ERROR "GDP visual content is missing from src/game")
endif()
foreach(gdp_visual_generator IN ITEMS
    "${GDP_SOURCE_DIR}/src/game/visual/EnemyGenerator.cpp"
    "${GDP_SOURCE_DIR}/src/game/visual/EnemyGenerator.hpp"
    "${GDP_SOURCE_DIR}/src/game/visual/RoomMaterialGenerator.cpp"
    "${GDP_SOURCE_DIR}/src/game/visual/RoomMaterialGenerator.hpp"
)
    file(READ "${gdp_visual_generator}" gdp_visual_generator_contents)
    if(NOT gdp_visual_generator_contents MATCHES
            "namespace[ \t]+gdp::game::visual[ \t{]"
        OR gdp_visual_generator_contents MATCHES
            "namespace[ \t]+gdp::procedural[ \t{]")
        file(RELATIVE_PATH relative_source
            "${GDP_SOURCE_DIR}" "${gdp_visual_generator}")
        message(FATAL_ERROR
            "GDP visual generators must be owned by gdp::game::visual: ${relative_source}"
        )
    endif()
endforeach()
file(READ "${GDP_SOURCE_DIR}/src/game/visual/EnemyGenerator.hpp"
    gdp_enemy_generator_header)
file(READ "${GDP_SOURCE_DIR}/src/game/visual/RoomMaterialGenerator.hpp"
    gdp_room_material_generator_header)
if(NOT gdp_enemy_generator_header MATCHES "wkc::procedural::MeshRecipe"
    OR NOT gdp_room_material_generator_header MATCHES
        "wkc::procedural::MaterialFamily")
    message(FATAL_ERROR
        "GDP visual generators must consume the qualified procedural foundation"
    )
endif()
set(gdp_renderer_header "${GDP_SOURCE_DIR}/src/game/visual/GdpRenderer.hpp")
set(gdp_render_data_header "${GDP_SOURCE_DIR}/src/game/visual/GdpRenderData.hpp")
if(NOT EXISTS "${gdp_renderer_header}")
    message(FATAL_ERROR "GDP concrete renderer is missing from src/game/visual")
endif()
if(NOT EXISTS "${gdp_render_data_header}")
    message(FATAL_ERROR "GDP render data contract is missing from src/game/visual")
endif()
file(READ "${gdp_renderer_header}" gdp_renderer_contents)
if(NOT gdp_renderer_contents MATCHES "class[ \t]+GdpRenderer[ \t{:]"
    OR gdp_renderer_contents MATCHES "class[ \t]+Renderer[ \t{:]" )
    message(FATAL_ERROR "GDP concrete renderer must use the explicit GdpRenderer name")
endif()
if(NOT gdp_renderer_contents MATCHES
        "namespace[ \t]+gdp::game::visual[ \t{]"
    OR gdp_renderer_contents MATCHES
        "namespace[ \t]+gdp::render[ \t{]")
    message(FATAL_ERROR "GDP concrete renderer must be owned by gdp::game::visual")
endif()
if(NOT gdp_renderer_contents MATCHES
        "#include[ \t]+\"game/visual/GdpRenderData.hpp\"")
    message(FATAL_ERROR "GDP renderer must consume the render data contract")
endif()
if(NOT gdp_renderer_contents MATCHES "wkc::render::Texture")
    message(FATAL_ERROR "GDP renderer must consume qualified render foundation types")
endif()
if(gdp_renderer_contents MATCHES "struct[ \t]+HudRenderData"
    OR gdp_renderer_contents MATCHES "enum[ \t]+class[ \t]+EnemyVisualRole")
    message(FATAL_ERROR "GDP renderer class header must not own presentation DTOs")
endif()
file(READ "${gdp_render_data_header}" gdp_render_data_contents)
if(NOT gdp_render_data_contents MATCHES
        "namespace[ \t]+gdp::game::visual[ \t{]"
    OR gdp_render_data_contents MATCHES
        "namespace[ \t]+gdp::render[ \t{]")
    message(FATAL_ERROR "GDP presentation DTOs must be owned by gdp::game::visual")
endif()
if(NOT gdp_render_data_contents MATCHES "struct[ \t]+HudRenderData"
    OR NOT gdp_render_data_contents MATCHES "struct[ \t]+RoomRenderData"
    OR NOT gdp_render_data_contents MATCHES
        "enum[ \t]+class[ \t]+EnemyVisualRole")
    message(FATAL_ERROR "GDP render data contract is incomplete")
endif()
if(EXISTS "${GDP_SOURCE_DIR}/src/render/Renderer.cpp"
    OR EXISTS "${GDP_SOURCE_DIR}/src/render/Renderer.hpp")
    message(FATAL_ERROR "Generic render directory must not own the GDP renderer")
endif()

foreach(generic_visual_api IN ITEMS
    "${GDP_SOURCE_DIR}/src/procedural/AssetRecipe.hpp"
    "${GDP_SOURCE_DIR}/src/render/Texture.hpp"
)
    file(READ "${generic_visual_api}" generic_visual_contents)
    if(generic_visual_contents MATCHES "Worker|Industrial|CeilingFixture")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${generic_visual_api}")
        message(FATAL_ERROR
            "Generic visual API owns GDP content: ${relative_source}"
        )
    endif()
endforeach()

file(GLOB procedural_foundation_sources
    "${GDP_SOURCE_DIR}/src/procedural/*.cpp"
    "${GDP_SOURCE_DIR}/src/procedural/*.hpp"
)
foreach(source IN LISTS procedural_foundation_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+wkc::procedural[ \t{]"
        OR contents MATCHES "namespace[ \t]+gdp::procedural[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "WKC Forge procedural API must use wkc::procedural: ${relative_source}"
        )
    endif()
    if(contents MATCHES "#include[ \t]+\"(game|gameplay|world|entities|audio)/"
        OR contents MATCHES
            "Worker|Guard|Officer|Engineer|IndustrialRobot|RoomMaterial")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "Procedural foundation owns GDP visual content: ${relative_source}"
        )
    endif()
endforeach()

if(EXISTS "${GDP_SOURCE_DIR}/src/procedural/AssetCache.hpp")
    message(FATAL_ERROR "Generic resource cache must not return to procedural")
endif()
set(resource_cache "${GDP_SOURCE_DIR}/src/resources/AssetCache.hpp")
if(NOT EXISTS "${resource_cache}")
    message(FATAL_ERROR "WKC Forge resource cache is missing from src/resources")
endif()
file(READ "${resource_cache}" resource_cache_contents)
if(NOT resource_cache_contents MATCHES
        "namespace[ \t]+wkc::resources[ \t{]"
    OR resource_cache_contents MATCHES
        "namespace[ \t]+gdp::procedural[ \t{]")
    message(FATAL_ERROR "WKC Forge resource cache must use wkc::resources")
endif()
if(resource_cache_contents MATCHES
        "#include[ \t]+\"(game|gameplay|world|entities|audio|procedural|render)/"
    OR resource_cache_contents MATCHES
        "Worker|Guard|Officer|Engineer|IndustrialRobot|RoomMaterial")
    message(FATAL_ERROR "WKC Forge resource cache depends on GDP content")
endif()
if(NOT gdp_renderer_contents MATCHES "wkc::resources::AssetCache")
    message(FATAL_ERROR "GDP renderer must consume the WKC Forge resource API")
endif()

file(GLOB render_foundation_sources
    "${GDP_SOURCE_DIR}/src/render/*.cpp"
    "${GDP_SOURCE_DIR}/src/render/*.hpp"
)
foreach(source IN LISTS render_foundation_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+wkc::render[ \t{]"
        OR contents MATCHES "namespace[ \t]+gdp::render[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "WKC Forge render API must use wkc::render: ${relative_source}"
        )
    endif()
    if(contents MATCHES "#include[ \t]+\"(game|gameplay|world|entities|audio|procedural)/")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "Render foundation depends on GDP content: ${relative_source}"
        )
    endif()
endforeach()

if(EXISTS "${GDP_SOURCE_DIR}/src/audio/AudioSystem.cpp"
    OR EXISTS "${GDP_SOURCE_DIR}/src/audio/AudioSystem.hpp"
    OR EXISTS "${GDP_SOURCE_DIR}/src/audio/ProceduralAudio.cpp"
    OR EXISTS "${GDP_SOURCE_DIR}/src/audio/ProceduralAudio.hpp")
    message(FATAL_ERROR "GDP audio recipes and facade must not return to src/audio")
endif()
foreach(gdp_audio_source IN ITEMS
    "${GDP_SOURCE_DIR}/src/game/audio/AudioRecipes.cpp"
    "${GDP_SOURCE_DIR}/src/game/audio/AudioRecipes.hpp"
    "${GDP_SOURCE_DIR}/src/game/audio/AudioSystem.cpp"
    "${GDP_SOURCE_DIR}/src/game/audio/AudioSystem.hpp"
)
    if(NOT EXISTS "${gdp_audio_source}")
        message(FATAL_ERROR "GDP audio source is missing: ${gdp_audio_source}")
    endif()
endforeach()
if(NOT EXISTS "${GDP_SOURCE_DIR}/src/audio/AudioMixer.cpp"
    OR NOT EXISTS "${GDP_SOURCE_DIR}/src/audio/AudioMixer.hpp")
    message(FATAL_ERROR "WKC Forge generic audio mixer is missing")
endif()
file(READ "${GDP_SOURCE_DIR}/src/game/audio/AudioSystem.cpp" gdp_audio_system_source)
file(READ "${GDP_SOURCE_DIR}/src/game/audio/AudioSystem.hpp" gdp_audio_system_header)
if(gdp_audio_system_source MATCHES "SDL_"
    OR gdp_audio_system_header MATCHES "SDL_")
    message(FATAL_ERROR "GDP audio facade must delegate SDL to WKC Forge")
endif()
file(GLOB audio_foundation_sources
    "${GDP_SOURCE_DIR}/src/audio/*.cpp"
    "${GDP_SOURCE_DIR}/src/audio/*.hpp"
)
foreach(source IN LISTS audio_foundation_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+wkc::audio[ \t{]"
        OR contents MATCHES "namespace[ \t]+gdp::audio[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "WKC Forge audio API must use the wkc::audio namespace: ${relative_source}"
        )
    endif()
    if(contents MATCHES "#include[ \t]+\"(game|gameplay|world|entities)/"
        OR contents MATCHES
            "WeaponFire|EnemyHit|EnemyDeath|PropDestroyed|Pickup|RoomMaterial")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "WKC Forge audio foundation owns GDP content: ${relative_source}"
        )
    endif()
endforeach()
file(READ "${GDP_SOURCE_DIR}/src/game/audio/AudioRecipes.hpp" gdp_audio_recipes_header)
if(NOT gdp_audio_recipes_header MATCHES "wkc::audio::AudioRecipe")
    message(FATAL_ERROR "GDP audio recipes must produce the WKC Forge recipe contract")
endif()
if(NOT gdp_audio_system_header MATCHES "wkc::audio::AudioMixer")
    message(FATAL_ERROR "GDP audio facade must own the WKC Forge mixer")
endif()

file(READ "${game_application}" application_header)
if(NOT application_header MATCHES "namespace[ \t]+gdp::game")
    message(FATAL_ERROR "GDP Application must use the gdp::game namespace")
endif()
if(NOT application_header MATCHES "wkc::platform::Input"
    OR application_header MATCHES "gdp::platform::Input")
    message(FATAL_ERROR "GDP Application must consume the WKC Forge input API")
endif()
if(NOT application_header MATCHES "wkc::core::FixedStepClock"
    OR application_header MATCHES "gdp::core::FixedStepClock")
    message(FATAL_ERROR "GDP Application must consume the WKC Forge Time API")
endif()

file(GLOB core_sources
    "${core_directory}/*.cpp"
    "${core_directory}/*.hpp"
)
foreach(source IN LISTS core_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "#include[ \t]*<SDL3/")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "Core must not include SDL directly: ${relative_source}"
        )
    endif()
    string(REGEX MATCHALL "#include[ \t]+\"[^\"]+\"" local_includes "${contents}")
    foreach(include IN LISTS local_includes)
        if(NOT include MATCHES "\"core/[^\"]+\"")
            file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
            message(FATAL_ERROR
                "Core boundary violation in ${relative_source}: ${include}"
            )
        endif()
    endforeach()
endforeach()

foreach(generic_directory IN ITEMS
    core
    platform
    render
    procedural
    resources
    audio
)
    file(GLOB generic_sources
        "${GDP_SOURCE_DIR}/src/${generic_directory}/*.cpp"
        "${GDP_SOURCE_DIR}/src/${generic_directory}/*.hpp"
    )
    foreach(source IN LISTS generic_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
                "#include[ \t]+\"(game|gameplay|world|entities)/")
            file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
            message(FATAL_ERROR
                "WKC Forge source depends on GDP layer: ${relative_source}"
            )
        endif()
    endforeach()
endforeach()

file(GLOB gameplay_sources
    "${GDP_SOURCE_DIR}/src/gameplay/*.cpp"
    "${GDP_SOURCE_DIR}/src/gameplay/*.hpp"
)
foreach(source IN LISTS gameplay_sources)
    file(READ "${source}" contents)
    if(NOT contents MATCHES "namespace[ \t]+gdp::gameplay[ \t{]"
        OR contents MATCHES "namespace[ \t]+wkc::gameplay[ \t{]")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "GDP gameplay must remain in gdp::gameplay: ${relative_source}"
        )
    endif()
endforeach()
