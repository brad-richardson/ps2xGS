#pragma once

// G5 proof target: factory for the patched-cpu backend. The backend class
// itself is the upstream GSCpuBackend compiled from build-time patched
// scratch sources (see cmake/apply_g5_patches.cmake) with
// -DGSCpuBackend=GSPatchedCpuBackend, so the pristine `cpu` backend and the
// submodule stay untouched. This header exposes only the creator, keeping
// the patched header out of pristine translation units.

#include "runtime/gs/gs_backend.h"

#include <memory>

std::unique_ptr<GSRasterBackend> createPatchedCpuBackend();
