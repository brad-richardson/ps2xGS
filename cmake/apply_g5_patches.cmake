# G5 build-time patch applier. Copies the pinned upstream CPU-backend
# sources to a scratch tree, applies upstream-patches/*.patch with
# `patch -p1`, stages the results for the proof target. The submodule
# under upstream/ is never modified; a failed patch hunk fails the build.
#
# Invoked as:
#   cmake -DWORK=<scratch-workdir> -DSRC_DIR=<repo-root> \
#         -DPATCH_EXE=<patch-bin> -DSTAGE_DIR=<stage-root> \
#         -P cmake/apply_g5_patches.cmake

file(MAKE_DIRECTORY "${WORK}/upstream/ps2xRuntime/src/lib/gs")
file(MAKE_DIRECTORY "${WORK}/upstream/ps2xRuntime/include/runtime/gs")
file(MAKE_DIRECTORY "${STAGE_DIR}/include/runtime/gs")
file(MAKE_DIRECTORY "${STAGE_DIR}/src")

file(COPY_FILE
    "${SRC_DIR}/upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp"
    "${WORK}/upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp")
file(COPY_FILE
    "${SRC_DIR}/upstream/ps2xRuntime/include/runtime/gs/gs_cpu_backend.h"
    "${WORK}/upstream/ps2xRuntime/include/runtime/gs/gs_cpu_backend.h")

foreach(p clut-cache rmw-lookup present-scratch)
    execute_process(
        COMMAND "${PATCH_EXE}" -p1 -i "${SRC_DIR}/upstream-patches/${p}.patch"
        WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "G5: patch ${p}.patch failed to apply:\n${out}\n${err}")
    endif()
    message(STATUS "G5: applied ${p}.patch: ${out}")
endforeach()

file(COPY_FILE
    "${WORK}/upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp"
    "${STAGE_DIR}/src/gs_cpu_backend.cpp")
file(COPY_FILE
    "${WORK}/upstream/ps2xRuntime/include/runtime/gs/gs_cpu_backend.h"
    "${STAGE_DIR}/include/runtime/gs/gs_cpu_backend.h")
