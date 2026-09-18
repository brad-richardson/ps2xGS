#include "backend_factory.h"

#include "runtime/gs/gs_cpu_backend.h"

#include <stdexcept>

std::unique_ptr<GSRasterBackend> createRasterBackend(const std::string &name)
{
    if (name == "cpu")
        return std::make_unique<GSCpuBackend>();
    throw std::runtime_error("unknown backend '" + name + "' (G0 only builds 'cpu')");
}
