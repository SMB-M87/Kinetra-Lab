# The probe is kinetra_i4sim_tests; its catalog case discovers and loads every bundled
# blueprint. doctest reports to standard output, so failures are matched there too.
set(probe_case "--test-case=the bundled catalog holds five blueprints that all load")
# Start from an empty directory: a copy left by an earlier run would keep blueprints that
# have since been removed from the catalog.
file(REMOVE_RECURSE "${TEST_DIRECTORY}")
file(MAKE_DIRECTORY "${TEST_DIRECTORY}/bundle" "${TEST_DIRECTORY}/unrelated" "${TEST_DIRECTORY}/missing")
file(COPY "${PROBE}" DESTINATION "${TEST_DIRECTORY}/bundle")
file(COPY "${RESOURCES}/" DESTINATION "${TEST_DIRECTORY}/bundle/i4sim-blueprints")
get_filename_component(probe_name "${PROBE}" NAME)
execute_process(COMMAND "${TEST_DIRECTORY}/bundle/${probe_name}" "${probe_case}"
    WORKING_DIRECTORY "${TEST_DIRECTORY}/unrelated"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
# Requiring one passed case also catches a renamed case, which doctest would skip silently.
if(NOT result EQUAL 0 OR NOT output MATCHES "test cases: +1 \\| +1 passed")
    message(FATAL_ERROR "Relocated blueprint discovery failed: ${output}${error}")
endif()
# An executable without its adjacent assets must fail rather than accidentally
# reading the original build catalog and giving a false positive relocation test.
file(COPY "${PROBE}" DESTINATION "${TEST_DIRECTORY}/missing")
execute_process(COMMAND "${TEST_DIRECTORY}/missing/${probe_name}" "${probe_case}"
    WORKING_DIRECTORY "${TEST_DIRECTORY}/unrelated"
    RESULT_VARIABLE missing_result OUTPUT_VARIABLE missing_output ERROR_VARIABLE missing_error)
if(missing_result EQUAL 0 OR NOT "${missing_output}${missing_error}" MATCHES "i4sim-blueprints")
    message(FATAL_ERROR "Missing bundle assets did not produce a useful error: ${missing_output}${missing_error}")
endif()
message(STATUS "${output}")
