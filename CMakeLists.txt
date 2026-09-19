cmake_minimum_required(VERSION 3.21)
project(ps2xGS CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# G0 step 2: the pinned upstream CPU rasterizer, built standalone.
# Only what the GS backend needs goes into this target:
#   - upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp
#   - upstream/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp
# with upstream/ps2xRuntime/include on the path. In particular
# gs_frontend.cpp and the runtime (raylib) are NOT compiled here;
# gs_cpu_backend.cpp only #includes ps2_log.h without using its
# runtime-log machinery, so no shim is needed (see docs/reports/G0.md).
add_library(ps2xgs_upstream_cpu STATIC
    upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp
    upstream/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp
)
target_include_directories(ps2xgs_upstream_cpu PUBLIC
    upstream/ps2xRuntime/include
)

# G0 steps 3-6: capture format + recording backend. Tools (gsreplay,
# gsgen, gscensus) and tests link this.
add_library(ps2xgs_harness STATIC
    harness/gscap.cpp
    harness/recording_backend.cpp
)
target_include_directories(ps2xgs_harness PUBLIC
    harness
    upstream/ps2xRuntime/include
)
target_link_libraries(ps2xgs_harness PUBLIC ps2xgs_upstream_cpu)

# G0 step 4: replay + diff. The backend choice lives in
# harness/backend_factory.cpp so S1 adds "vulkan" there without
# touching the replay logic.
add_executable(gsreplay
    harness/gsreplay.cpp
    harness/backend_factory.cpp
    harness/strict_backend.cpp
    harness/spike_backend.cpp
)
target_link_libraries(gsreplay PRIVATE ps2xgs_harness)

# G0 step 5: synthetic capture generator.
add_executable(gsgen harness/gsgen.cpp)
target_link_libraries(gsgen PRIVATE ps2xgs_harness)

# G0 step 6: feature census.
add_executable(gscensus census/gscensus.cpp)
target_link_libraries(gscensus PRIVATE ps2xgs_harness)

# G0 step 7: CTest suite.
enable_testing()

# Captures and anything over 5 MB live outside the repo; the suite
# (re)generates the synthetic set there. Override with
# -DPS2XGS_SYNTH_DIR=<dir>.
set(PS2XGS_SYNTH_DIR "/Volumes/Extreme SSD/ps2xgs/synth" CACHE PATH
    "Directory for (generated) synthetic captures")
set(PS2XGS_CENSUS_JSON "${CMAKE_BINARY_DIR}/census.json" CACHE FILEPATH
    "Census JSON written by the test suite")
set(PS2XGS_CENSUS_MD "${CMAKE_BINARY_DIR}/census.md" CACHE FILEPATH
    "Census Markdown written by the test suite")

add_executable(test_gsregs tests/test_gsregs.cpp)
target_link_libraries(test_gsregs PRIVATE ps2xgs_harness)
add_test(NAME gsregs-roundtrip COMMAND test_gsregs)

add_executable(test_gscap_roundtrip tests/test_gscap_roundtrip.cpp)
target_link_libraries(test_gscap_roundtrip PRIVATE ps2xgs_harness)
add_test(NAME gscap-roundtrip COMMAND test_gscap_roundtrip)

add_test(NAME gen-synth COMMAND gsgen "${PS2XGS_SYNTH_DIR}")
set_tests_properties(gen-synth PROPERTIES FIXTURES_SETUP synth)

add_test(NAME identity-replay
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/replay_all.sh"
            "${PS2XGS_SYNTH_DIR}" "$<TARGET_FILE:gsreplay>")
set_tests_properties(identity-replay PROPERTIES FIXTURES_REQUIRED synth)

add_test(NAME run-census
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/run_census.sh"
            "$<TARGET_FILE:gscensus>" "${PS2XGS_SYNTH_DIR}"
            "${PS2XGS_CENSUS_JSON}" "${PS2XGS_CENSUS_MD}")
set_tests_properties(run-census PROPERTIES
    FIXTURES_REQUIRED synth
    FIXTURES_SETUP census-out)

add_test(NAME census-features
    COMMAND python3 "${CMAKE_SOURCE_DIR}/tests/check_census.py" "${PS2XGS_CENSUS_JSON}")
set_tests_properties(census-features PROPERTIES FIXTURES_REQUIRED census-out)

# G2 step 5: strict sensitivity (mip diffs), strict specificity
# (controls exact), cpu identity on new captures.
add_test(NAME strict-diffs-on-mip
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/check_strict_diffs.sh"
            "${PS2XGS_SYNTH_DIR}" "$<TARGET_FILE:gsreplay>")
set_tests_properties(strict-diffs-on-mip PROPERTIES FIXTURES_REQUIRED synth)

add_test(NAME strict-exact-on-control
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/check_strict_exact.sh"
            "${PS2XGS_SYNTH_DIR}" "$<TARGET_FILE:gsreplay>")
set_tests_properties(strict-exact-on-control PROPERTIES FIXTURES_REQUIRED synth)

add_test(NAME cpu-identity-on-new-captures
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/check_cpu_new.sh"
            "${PS2XGS_SYNTH_DIR}" "$<TARGET_FILE:gsreplay>")
set_tests_properties(cpu-identity-on-new-captures PROPERTIES FIXTURES_REQUIRED synth)

# G3 step 1: Present fbp==0 fallback heuristic pins.
add_executable(test_present_heuristic tests/test_present_heuristic.cpp)
target_link_libraries(test_present_heuristic PRIVATE ps2xgs_harness)
add_test(NAME present-heuristic
    COMMAND test_present_heuristic "${PS2XGS_SYNTH_DIR}")
set_tests_properties(present-heuristic PROPERTIES FIXTURES_REQUIRED synth)

# G3 steps 2-3: spike backend identity over every capture.
add_test(NAME spike-identity
    COMMAND sh "${CMAKE_SOURCE_DIR}/tests/replay_spike.sh"
            "${PS2XGS_SYNTH_DIR}" "$<TARGET_FILE:gsreplay>")
set_tests_properties(spike-identity PROPERTIES FIXTURES_REQUIRED synth)
