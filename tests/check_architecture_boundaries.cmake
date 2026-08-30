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
