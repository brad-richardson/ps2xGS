# backend

S1: this directory will hold `GSVulkanBackend : GSRasterBackend`
(+ `shaders/`). Nothing here in G0 — the only backend is the pinned
upstream CPU backend, built standalone as the `ps2xgs_upstream_cpu`
CMake target (see `../CMakeLists.txt`).
