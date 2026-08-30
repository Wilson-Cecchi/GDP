if(NOT DEFINED GDP_SOURCE_DIR)
    message(FATAL_ERROR "GDP_SOURCE_DIR was not provided")
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

file(READ "${core_directory}/Seed.hpp" core_seed_header)
if(core_seed_header MATCHES "enum[ \t]+class[ \t]+SeedDomain")
    message(FATAL_ERROR "core/Seed.hpp must not own GDP seed domains")
endif()
if(NOT EXISTS "${GDP_SOURCE_DIR}/src/game/SeedDomains.hpp")
    message(FATAL_ERROR "GDP seed domain catalog is missing from src/game")
endif()

set(game_visual_assets "${GDP_SOURCE_DIR}/src/game/visual/VisualAssets.hpp")
if(NOT EXISTS "${game_visual_assets}")
    message(FATAL_ERROR "GDP visual content is missing from src/game")
endif()
set(gdp_renderer_header "${GDP_SOURCE_DIR}/src/game/visual/GdpRenderer.hpp")
if(NOT EXISTS "${gdp_renderer_header}")
    message(FATAL_ERROR "GDP concrete renderer is missing from src/game/visual")
endif()
file(READ "${gdp_renderer_header}" gdp_renderer_contents)
if(NOT gdp_renderer_contents MATCHES "class[ \t]+GdpRenderer[ \t{:]"
    OR gdp_renderer_contents MATCHES "class[ \t]+Renderer[ \t{:]" )
    message(FATAL_ERROR "GDP concrete renderer must use the explicit GdpRenderer name")
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
    if(contents MATCHES "#include[ \t]+\"(game|gameplay|world|entities|audio)/"
        OR contents MATCHES
            "Worker|Guard|Officer|Engineer|IndustrialRobot|RoomMaterial")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "Procedural foundation owns GDP visual content: ${relative_source}"
        )
    endif()
endforeach()

file(GLOB render_foundation_sources
    "${GDP_SOURCE_DIR}/src/render/*.cpp"
    "${GDP_SOURCE_DIR}/src/render/*.hpp"
)
foreach(source IN LISTS render_foundation_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "#include[ \t]+\"(game|gameplay|world|entities|audio|procedural)/")
        file(RELATIVE_PATH relative_source "${GDP_SOURCE_DIR}" "${source}")
        message(FATAL_ERROR
            "Render foundation depends on GDP content: ${relative_source}"
        )
    endif()
endforeach()

file(READ "${game_application}" application_header)
if(NOT application_header MATCHES "namespace[ \t]+gdp::game")
    message(FATAL_ERROR "GDP Application must use the gdp::game namespace")
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
