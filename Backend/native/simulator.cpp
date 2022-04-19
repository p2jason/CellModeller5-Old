#include "simulator.h"

#include "shader_compiler.h"

#include <fstream>

#include <thread>
#include <chrono>

static Simulator::CPUState allocateNewCPUState(uint32_t size);
static void freeCPUState(Simulator::CPUState& state);

static Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size);
static void freeGPUState(Simulator& simulator, Simulator::GPUState& state);

const char* COLLISION_SHADER = R"Boo(#version 450

layout(set = 0, binding = 0) readonly buffer Positions {
	vec3[] positions;
};

layout(set = 0, binding = 1) readonly buffer Rotations {
	vec2[] rotations;
};

layout(set = 0, binding = 2) readonly buffer Sizes {
	vec2[] sizes;
};

void main() {
	
}

)Boo";

Result<void> initSimulator(Simulator* simulator, bool withDebug)
{
	CM_PROPAGATE_ERROR(initGPUContext(&simulator->gpuContext, withDebug));
	CM_PROPAGATE_ERROR(initGPUDevice(&simulator->gpuDevice, simulator->gpuContext));

	startupShaderCompiler();

	//Create fence for waiting on queue submissions
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	VK_THROW(vkCreateFence(simulator->gpuDevice.device, &fenceCI, nullptr, &simulator->submitFinishedFence));

	//Create pipelines
	PipelineParameters pipelineParams = {};
	pipelineParams.descriptorBindings.push_back(VkDescriptorSetLayoutBinding{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });

	pipelineParams.pushConstans.push_back(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) });

	CM_TRY(auto& collisionShader, compileShaderSpirV(COLLISION_SHADER, "collision_shader"));
	CM_TRY(auto& shaderPipeline, createShaderPipeline(simulator->gpuDevice, collisionShader, pipelineParams));

	//Print details
	const VkPhysicalDeviceProperties& properties = simulator->gpuDevice.properties;

	printf("Vulkan Device:\n");
	printf("    Name:  %s\n", properties.deviceName);
	printf("    API Version: %d.%d.%d\n", VK_API_VERSION_MAJOR(properties.apiVersion),
										  VK_API_VERSION_MINOR(properties.apiVersion),
										  VK_API_VERSION_PATCH(properties.apiVersion));

	//Set the initial state of the simulation
	simulator->cpuState = allocateNewCPUState(1024);
	CM_TRY(simulator->gpuState, allocateNewGPUState(*simulator, 1024));

	for (int i = 0; i < 11; ++i)
	{
		simulator->cpuState.positions[i] = { 0.f, 0.0f, 2.6f * (6 - i) };
		simulator->cpuState.rotations[i] = { 3.14159265359f / 2.0f, 0.0f };
		simulator->cpuState.sizes[i] = { 3.0f, 0.5f };
		simulator->cpuState.colors[i] = 0xFF0000FF;
	}

	simulator->cellCount = 11;

	return Result<void>();
}

void deinitSimulator(Simulator& simulator)
{
	vkDestroyFence(simulator.gpuDevice.device, simulator.submitFinishedFence, nullptr);

	freeCPUState(simulator.cpuState);
	freeGPUState(simulator, simulator.gpuState);

	deinitGPUDevice(simulator.gpuDevice);
	deinitGPUContext(simulator.gpuContext);
}

Result<void> stepSimulator(Simulator& simulator)
{
	simulator.stepIndex++;

	//Temp:
	if (simulator.stepIndex > 5) return CM_ERROR_MESSAGE("No Frodo for you!");

	GPUDevice& device = simulator.gpuDevice;

	VK_THROW(vkResetCommandPool(device.device, device.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_THROW(vkBeginCommandBuffer(device.commandBuffer, &beginInfo));

	

	VK_THROW(vkEndCommandBuffer(device.commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &device.commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	
	VK_THROW(vkQueueSubmit(device.commandQueue, 1, &submitInfo, simulator.submitFinishedFence));

	VK_THROW(vkWaitForFences(device.device, 1, &simulator.submitFinishedFence, VK_TRUE, UINT64_MAX));

	return Result<void>();
}

Result<void> writeSimlatorStateToStepFile(Simulator& simulator, std::string filepath)
{
	return Result<void>();
}

vec3 directionFromAngles(vec2 rotation)
{
	return { sin(rotation.y), cos(rotation.x), cos(rotation.y) };
}

Result<void> writeSimlatorStateToVizFile(Simulator& simulator, std::string filepath)
{
	std::ofstream out(filepath, std::ios::binary | std::ios::trunc);

	if (!out)
	{
		CM_ERROR_MESSAGE("Failed to write to viz file: " + filepath);
	}

	uint8_t buffer[8 * sizeof(float) + sizeof(uint32_t)];

	out.write((const char*)&simulator.cellCount, sizeof(uint32_t));
	
	for (uint32_t i = 0; i < simulator.cellCount; ++i)
	{
		union
		{
			float* bufferAsFloat;
			uint32_t* bufferAsUInt;
		};

		bufferAsFloat = (float*)buffer;

		vec3 dir = directionFromAngles(simulator.cpuState.rotations[i]);

		*(bufferAsFloat++) = simulator.cpuState.positions[i].x;
		*(bufferAsFloat++) = simulator.cpuState.positions[i].y;
		*(bufferAsFloat++) = simulator.cpuState.positions[i].z;

		*(bufferAsFloat++) = dir.x;
		*(bufferAsFloat++) = dir.y;
		*(bufferAsFloat++) = dir.z;

		*(bufferAsFloat++) = simulator.cpuState.sizes[i].x;
		*(bufferAsFloat++) = simulator.cpuState.sizes[i].y;
		*(bufferAsUInt++) = simulator.cpuState.colors[i];

		out.write((const char*)buffer, sizeof(buffer));
	}

	out.close();

	return Result<void>();
}

Simulator::CPUState allocateNewCPUState(uint32_t size)
{
	Simulator::CPUState state = {};
	state.positions = new vec3[size];
	state.rotations = new vec2[size];
	state.sizes = new vec2[size];
	state.colors = new uint32_t[size];

	return state;
}

void freeCPUState(Simulator::CPUState& state)
{
	delete[] state.positions;
	delete[] state.rotations;
	delete[] state.sizes;
	delete[] state.colors;
}

Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size)
{
	const VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	Simulator::GPUState state = {};
	CM_TRY(state.positions, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec3), usageFlags, memoryProperties));
	CM_TRY(state.rotations, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec2), usageFlags, memoryProperties));
	CM_TRY(state.sizes, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec2), usageFlags, memoryProperties));

	return state;
}

void freeGPUState(Simulator& simulator, Simulator::GPUState& state)
{
	destroyGPUBuffer(simulator.gpuDevice, state.positions);
	destroyGPUBuffer(simulator.gpuDevice, state.rotations);
	destroyGPUBuffer(simulator.gpuDevice, state.sizes);
}