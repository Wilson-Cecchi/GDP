if(NOT DEFINED GDP_EXECUTABLE)
    message(FATAL_ERROR "GDP_EXECUTABLE was not provided")
endif()

set(ENV{SDL_VIDEODRIVER} "offscreen")
set(ENV{SDL_AUDIODRIVER} "dummy")
execute_process(
    COMMAND "${GDP_EXECUTABLE}" --frames 2
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
set(combined "${output}${error}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Graphical menu bootstrap failed (${result}):\n${combined}")
endif()
if(NOT combined MATCHES "Main menu initialized before world generation")
    message(FATAL_ERROR "Graphical bootstrap did not reach the main menu:\n${combined}")
endif()
if(combined MATCHES "Generated world seed")
    message(FATAL_ERROR "World generation ran before Start Run:\n${combined}")
endif()
