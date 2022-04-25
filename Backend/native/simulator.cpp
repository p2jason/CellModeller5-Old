#include "simulator.h"

#include "shader_compiler.h"

#include <fstream>
#include <iostream>

#include <thread>
#include <chrono>

static Simulator::CPUState allocateNewCPUState(uint32_t size);
static void freeCPUState(Simulator::CPUState& state);

static Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size);
static void freeGPUState(Simulator& simulator, Simulator::GPUState& state);

Result<void> initSimulator(Simulator* simulator, bool withDebug)
{
	CM_PROPAGATE_ERROR(initGPUContext(&simulator->gpuContext, withDebug));
	CM_PROPAGATE_ERROR(initGPUDevice(&simulator->gpuDevice, simulator->gpuContext));

	//Create fence for waiting on queue submissions
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	VK_THROW(vkCreateFence(simulator->gpuDevice.device, &fenceCI, nullptr, &simulator->submitFinishedFence));

	//Print details
	const VkPhysicalDeviceProperties& properties = simulator->gpuDevice.properties;

	printf("Vulkan Device:\n");
	printf("    Name:  %s\n", properties.deviceName);
	printf("    API Version: %d.%d.%d\n", VK_API_VERSION_MAJOR(properties.apiVersion),
										  VK_API_VERSION_MINOR(properties.apiVersion),
										  VK_API_VERSION_PATCH(properties.apiVersion));

	//Set the initial state of the simulation
	simulator->cellCapacity = 1024;

	simulator->cpuState = allocateNewCPUState(simulator->cellCapacity);
	CM_TRY(simulator->gpuState, allocateNewGPUState(*simulator, simulator->cellCapacity));

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

Result<void> importShaders(Simulator& simulator, ShaderImportCallback importCallback)
{
	//Create a single descriptor pool for all the shaders
	VkDevice deviceHandle = simulator.gpuDevice.device;

	VkDescriptorPoolSize poolSizes[1] = {};
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 };

	VkDescriptorPoolCreateInfo poolCI = {};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 1;
	poolCI.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
	poolCI.pPoolSizes = poolSizes;

	VK_THROW(vkCreateDescriptorPool(deviceHandle, &poolCI, nullptr, &simulator.descriptorPool));

	/*********** Collision detection shader ***********/
	PipelineParameters params = {};
	params.descriptorBindings.push_back({ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
	params.descriptorBindings.push_back({ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
	params.descriptorBindings.push_back({ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });

	params.pushConstans.push_back({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) });

	CM_TRY(auto& compiled, compileShaderSpirV(importCallback("shaders/collision_shader.glsl"), "shaders/collision_shader.glsl"));
	CM_TRY(simulator.collisionShader, createShaderPipeline(simulator.gpuDevice, compiled, params));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = simulator.descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &simulator.collisionShader.descSetLayout;

	VK_THROW(vkAllocateDescriptorSets(deviceHandle, &allocInfo, &simulator.collisionShaderDescSet));

	return Result<void>();
}

void deinitSimulator(Simulator& simulator)
{
	VkDevice deviceHandle = simulator.gpuDevice.device;

	if (simulator.descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(deviceHandle, simulator.descriptorPool, nullptr);
	}

	destroyShaderPipeline(simulator.gpuDevice, simulator.collisionShader);

	if (simulator.submitFinishedFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(deviceHandle, simulator.submitFinishedFence, nullptr);
	}

	freeCPUState(simulator.cpuState);
	freeGPUState(simulator, simulator.gpuState);

	deinitGPUDevice(simulator.gpuDevice);
	deinitGPUContext(simulator.gpuContext);
}

static uint32_t workgroupCount(uint32_t threadCount, uint32_t groupSize)
{
	uint32_t m = threadCount % groupSize;
	return (m ? (threadCount + groupSize - m) : threadCount) / groupSize;
}

Result<void> stepSimulator(Simulator& simulator)
{
	GPUDevice& device = simulator.gpuDevice;

	VK_THROW(vkResetCommandPool(device.device, device.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	///////////////////////////////////////////////////////////////
	// Update descriptor sets
	///////////////////////////////////////////////////////////////
	VkDescriptorBufferInfo bufferWriteInfos[3] = {};
	bufferWriteInfos[0].buffer = simulator.gpuState.positions.buffer;
	bufferWriteInfos[0].offset = 0;
	bufferWriteInfos[0].range = simulator.cellCount * sizeof(vec3);

	bufferWriteInfos[1].buffer = simulator.gpuState.rotations.buffer;
	bufferWriteInfos[1].offset = 0;
	bufferWriteInfos[1].range = simulator.cellCount * sizeof(vec2);

	bufferWriteInfos[2].buffer = simulator.gpuState.sizes.buffer;
	bufferWriteInfos[2].offset = 0;
	bufferWriteInfos[2].range = simulator.cellCount * sizeof(vec2);

	VkWriteDescriptorSet descSetWrites[3] = {};
	descSetWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descSetWrites[0].dstSet = simulator.collisionShaderDescSet;
	descSetWrites[0].dstBinding = 0;
	descSetWrites[0].dstArrayElement = 0;
	descSetWrites[0].descriptorCount = 1;
	descSetWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descSetWrites[0].pBufferInfo = &bufferWriteInfos[0];

	descSetWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descSetWrites[1].dstSet = simulator.collisionShaderDescSet;
	descSetWrites[1].dstBinding = 1;
	descSetWrites[1].dstArrayElement = 0;
	descSetWrites[1].descriptorCount = 1;
	descSetWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descSetWrites[1].pBufferInfo = &bufferWriteInfos[1];

	descSetWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descSetWrites[2].dstSet = simulator.collisionShaderDescSet;
	descSetWrites[2].dstBinding = 2;
	descSetWrites[2].dstArrayElement = 0;
	descSetWrites[2].descriptorCount = 1;
	descSetWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descSetWrites[2].pBufferInfo = &bufferWriteInfos[2];

	vkUpdateDescriptorSets(device.device, sizeof(descSetWrites) / sizeof(descSetWrites[0]), descSetWrites, 0, nullptr);

	///////////////////////////////////////////////////////////////
	// Perform a step
	///////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_THROW(vkBeginCommandBuffer(device.commandBuffer, &beginInfo));

	// Run collision detection and accumulate forces
	vkCmdBindPipeline(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipeline);
	vkCmdBindDescriptorSets(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipelineLayout, 0, 1, &simulator.collisionShaderDescSet, 0, nullptr);
	
	vkCmdPushConstants(device.commandBuffer, simulator.collisionShader.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &simulator.cellCount);

	vkCmdDispatch(device.commandBuffer, workgroupCount(simulator.cellCount, 64), 1, 1);

	VK_THROW(vkEndCommandBuffer(device.commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &device.commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	
	VK_THROW(vkQueueSubmit(device.commandQueue, 1, &submitInfo, simulator.submitFinishedFence));

	VK_THROW(vkWaitForFences(device.device, 1, &simulator.submitFinishedFence, VK_TRUE, UINT64_MAX));
	VK_THROW(vkResetFences(device.device, 1, &simulator.submitFinishedFence));

	return Result<void>();
}

Result<void> writeSimulatorStateToStepFile(Simulator& simulator, std::string filepath)
{
	return Result<void>();
}

vec3 directionFromAngles(vec2 rotation)
{
	return { sin(rotation.y), cos(rotation.x), cos(rotation.y) };
}

Result<void> writeSimulatorStateToVizFile(Simulator& simulator, std::string filepath)
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