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
