# Runs kinetra_capture for one screenshot and three clips, and checks the files: an i4sim
# clip, whose script sets motion settings through the debug menu; the Dotio debug clip, whose
# player is scripted against the session's world; and a showcase clip, whose blueprint lies
# in a subdirectory of the catalog and whose length is measured.
# Then plays the first seconds of the tour without writing them: the launcher, the start of
# Demo and its settings panel.
# Called by CTest as: cmake -DTOOL=<kinetra_capture> -DOUTPUT=<directory> -P smoke.cmake
file(REMOVE_RECURSE "${OUTPUT}")
set(showcase i4sim-showcase-junction-crossing-clip)
set(clips i4sim-debug-clip dotio-debug-clip ${showcase})
execute_process(COMMAND "${TOOL}" "${OUTPUT}" launcher ${clips} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "kinetra_capture failed with ${result}")
endif()
if(NOT EXISTS "${OUTPUT}/launcher.png")
    message(FATAL_ERROR "kinetra_capture did not write launcher.png")
endif()
foreach(clip ${clips})
    foreach(file frame-0001.png framerate)
        if(NOT EXISTS "${OUTPUT}/${clip}/${file}")
            message(FATAL_ERROR "kinetra_capture did not write ${clip}/${file}")
        endif()
    endforeach()
    file(GLOB frames "${OUTPUT}/${clip}/frame-*.png")
    list(LENGTH frames frame_count)
    if(frame_count LESS 2)
        message(FATAL_ERROR "kinetra_capture wrote ${frame_count} frames of ${clip}")
    endif()
endforeach()
file(READ "${OUTPUT}/launcher.png" signature LIMIT 8 HEX)
if(NOT signature STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "launcher.png is not a PNG file")
endif()
# Seven seconds of the tour: through the launcher into Demo and its open settings panel.
execute_process(COMMAND "${TOOL}" --tour-check 420 RESULT_VARIABLE result ERROR_VARIABLE report)
if(NOT result EQUAL 0 OR NOT report MATCHES "tour of 420 frames")
    message(FATAL_ERROR "kinetra_capture --tour-check failed with ${result}: ${report}")
endif()
