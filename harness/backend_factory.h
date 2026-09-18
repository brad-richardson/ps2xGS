#pragma once

// Backend factory: replay (and future tools) select a GSRasterBackend
// by name without touching replay logic. S1 adds "vulkan" here.

#include "runtime/gs/gs_backend.h"

#include <memory>
#include <string>

std::unique_ptr<GSRasterBackend> createRasterBackend(const std::string &name);
