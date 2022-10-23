#pragma once

#include "result.h"
#include "gpu_device.h"
#include "shader_compiler.h"

#include <cstdint>
#include <string>
#include <functional>

struct vec3 { float x; float y; float z; float padding0; };
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
		vec3* velocities = nullptr;
		uint32_t* colors = nullptr;
	};

	struct GPUState
	{
		GPUBuffer positions = {};
		GPUBuffer rotations = {};
		GPUBuffer sizes = {};
		GPUBuffer velocities = {};
	};

	/********* GPU stuff *********/
	GPUContext gpuContext;
	GPUDevice gpuDevice;

	VkFence submitFinishedFence = VK_NULL_HANDLE;
	VkQueryPool timingQueryPool = VK_NULL_HANDLE;

	/********* Simulation state *********/
	uint32_t cellCount = 0;
	uint32_t cellCapacity = 0;

	CPUState cpuState = {};
	GPUState cpuStateMemory = {};

	GPUState gpuStates[2] = {};
	bool gpuStateToggle = false;

	bool uploadStateOnNextStep = false;
	double lastStepTime = 0.0;

	/********* Shaders *********/
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	VkDescriptorSetLayout stateDescLayout = VK_NULL_HANDLE;
	VkDescriptorSet inputStateDescSet = VK_NULL_HANDLE;
	VkDescriptorSet outputStateDescSet = VK_NULL_HANDLE;

	ShaderPipeline collisionShader = {};

	/********* Miscellaneous *********/
	std::function<void()> queueWaitBegin = nullptr;
	std::function<void()> queueWaitEnd = nullptr;

	int compressionLevel = 2;
};

typedef std::function<std::string(const std::string&)> ShaderImportCallback;

Result<void> initSimulator(Simulator* simulator, bool withDebug = false);
Result<void> importShaders(Simulator& simulator, ShaderImportCallback importCallback);
void deinitSimulator(Simulator& simulator);

Result<void> stepSimulator(Simulator& simulator);
Result<void> writeSimulatorStateToStepFile(Simulator& simulator, std::string filepath);
Result<void> writeSimulatorStateToVizFile(Simulator& simulator, std::string filepath);