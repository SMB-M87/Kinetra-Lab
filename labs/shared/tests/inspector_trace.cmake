execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env SDL_VIDEODRIVER=invalid "${PROGRAM}" ${ARGS}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Inspector trace failed: ${diagnostics}")
endif()

string(REPLACE "\r\n" "\n" output "${output}")
string(STRIP "${output}" output)
string(REPLACE "\n" ";" records "${output}")
list(LENGTH records count)
if(NOT count EQUAL TICKS OR NOT output MATCHES "${EXPECTED}")
    message(FATAL_ERROR "Inspector trace missed rows or ticks")
endif()

set(tick 0)
foreach(record IN LISTS records)
    math(EXPR tick "${tick} + 1")
    if(NOT record MATCHES "^\\{.*\"groups\":.*\"id\":.*\"tick\":${tick}\\}$")
        message(FATAL_ERROR "Invalid inspector record for tick ${tick}: ${record}")
    endif()
endforeach()
