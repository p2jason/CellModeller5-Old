#pragma once

#include "result.h"
#include "gpu_device.h"
#include "shader_compiler.h"

#include <cstdint>

struct Simulator
{
	GPUContext gpuContext;
	GPUDevice gpuDevice;
};

Result<void> initSimulator(Simulator* simulator, bool withDebug = false);
void deinitSimulator(Simulator& simulator);