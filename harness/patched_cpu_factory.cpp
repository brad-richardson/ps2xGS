// G5 proof target: compiled inside ps2xgs_patched_cpu with the patched
// include dir first and -DGSCpuBackend=GSPatchedCpuBackend, so this TU
// (and only this TU plus the staged gs_cpu_backend.cpp) sees the patched
// header and the renamed class.

#include "patched_cpu_backend.h"

#include "runtime/gs/gs_cpu_backend.h"

std::unique_ptr<GSRasterBackend> createPatchedCpuBackend()
{
    return std::make_unique<GSCpuBackend>();
}
