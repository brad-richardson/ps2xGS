#include "backend_factory.h"

#include "runtime/gs/gs_cpu_backend.h"
#include "spike_backend.h"
#include "strict_backend.h"

#include <stdexcept>

std::unique_ptr<GSRasterBackend> createRasterBackend(const std::string &name)
{
    if (name == "cpu")
        return std::make_unique<GSCpuBackend>();
    if (name == "strict")
        return std::make_unique<GSStrictBackend>();
    if (name == "spike")
        return std::make_unique<GSSpikeBackend>();
    throw std::runtime_error("unknown backend '" + name + "' (expected 'cpu', 'strict' or 'spike')");
}
