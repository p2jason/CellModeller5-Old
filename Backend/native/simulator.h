#pragma once

#include "result.h"
#include "gpu_device.h"
#include "shader_compiler.h"

#include <cstdint>
#include <string>
#include <functional>

struct vec3 { float x; float y; float z; };
struct vec2 { float x; float y; };

struct Simulator
{
	struct CPUState
	{
		vec3* positions = nullptr;
		/* pitch, yaw */
		vec2* rotations = nullptr;
		/* length, radius */
		vec2* sizes = nullptr;
		uint32_t* colors = nullptr;
	};

	struct GPUState
	{
		GPUBuffer positions = {};
		GPUBuffer rotations = {};
		GPUBuffer sizes = {};
	};

	GPUContext gpuContext;
	GPUDevice gpuDevice;

	VkFence submitFinishedFence = VK_NULL_HANDLE;

	/********* Simulation state *********/
	uint32_t cellCount = 0;
	uint32_t cellCapacity = 0;

	CPUState cpuState = {};
	GPUState gpuState = {};

	/********* Shaders *********/
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	ShaderPipeline collisionShader = {};
	VkDescriptorSet collisionShaderDescSet = VK_NULL_HANDLE;
};

typedef std::function<std::string(const std::string&)> ShaderImportCallback;

Result<void> initSimulator(Simulator* simulator, bool withDebug = false);
Result<void> importShaders(Simulator& simulator, ShaderImportCallback importCallback);
void deinitSimulator(Simulator& simulator);

Result<void> stepSimulator(Simulator& simulator);
Result<void> writeSimulatorStateToStepFile(Simulator& simulator, std::string filepath);
Result<void> writeSimulatorStateToVizFile(Simulator& simulator, std::string filepath);