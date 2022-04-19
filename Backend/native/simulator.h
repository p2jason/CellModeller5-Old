#pragma once

#include "result.h"
#include "gpu_device.h"
#include "shader_compiler.h"

#include <cstdint>
#include <string>

struct vec3 { float x; float y; float z; };
struct vec2 { float x; float y; };

/* length, radius */

struct Simulator
{
	GPUContext gpuContext;
	GPUDevice gpuDevice;

	uint32_t cellCount = 0;

	struct CPUState
	{
		vec3* positions = nullptr;
		/* pitch, yaw */
		vec2* rotations = nullptr;
		/* length, radius */
		vec2* sizes = nullptr;
		uint32_t* colors = nullptr;
	} cpuState;

	struct GPUState
	{
		GPUBuffer positions = {};
		GPUBuffer rotations = {};
		GPUBuffer sizes = {};
	} gpuState;

	VkFence submitFinishedFence = VK_NULL_HANDLE;

	int stepIndex = 0;
};

Result<void> initSimulator(Simulator* simulator, bool withDebug = false);
void deinitSimulator(Simulator& simulator);

Result<void> stepSimulator(Simulator& simulator);
Result<void> writeSimlatorStateToStepFile(Simulator& simulator, std::string filepath);
Result<void> writeSimlatorStateToVizFile(Simulator& simulator, std::string filepath);