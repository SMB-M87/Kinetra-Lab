# Runs kinetra_capture for one screenshot and four clips, and checks the files: an i4sim
# clip, whose script sets motion settings through the debug menu; the Dotio debug clip, whose
# player is scripted against the session's world; a showcase clip, whose blueprint lies in a
# subdirectory of the catalog and whose length is measured; and the shared-UI clips of the
# settings panel and of the launcher, which a tour plays through the launcher into Kinetra's
# subdirectory. The launcher clip stops the tool unless Demo's SCENARIOS row returns to the
# scenario page with Doorway selected.
# Then plays the first seconds of the tour and verifies which Demo scenario starts.
# Called by CTest as: cmake -DTOOL=<kinetra_capture> -DOUTPUT=<directory> -P smoke.cmake
file(REMOVE_RECURSE "${OUTPUT}")
set(showcase i4sim-showcase-junction-crossing-clip)
set(clips i4sim-debug-clip dotio-debug-clip ${showcase})
execute_process(COMMAND "${TOOL}" "${OUTPUT}" launcher ${clips} ui-settings ui-launcher RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "kinetra_capture failed with ${result}")
endif()
if(NOT EXISTS "${OUTPUT}/launcher.png")
    message(FATAL_ERROR "kinetra_capture did not write launcher.png")
endif()
foreach(clip ${clips} kinetra/ui-settings kinetra/ui-launcher)
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
# Eight seconds of the tour: through the launcher and the Demo scenario menu.
execute_process(COMMAND "${TOOL}" --tour-check 480 RESULT_VARIABLE result ERROR_VARIABLE report)
if(NOT result EQUAL 0 OR NOT report MATCHES "tour of 480 frames" OR
   NOT report MATCHES "tour verified Demo Terrain")
    message(FATAL_ERROR "kinetra_capture --tour-check failed with ${result}: ${report}")
endif()
