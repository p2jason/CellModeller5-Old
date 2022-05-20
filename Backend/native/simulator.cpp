#include "simulator.h"

#include "shader_compiler.h"

#include <fstream>
#include <iostream>

struct GlobalConsts
{
	uint32_t cellCount = 0;
	float deltaTime = 0.0f;
};

enum class CopyDirection
{
	HostToDevice,
	DeviceToHpst
};

static Simulator::CPUState allocateNewCPUState(uint32_t size);
static void freeCPUState(Simulator::CPUState& state);

static Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size, bool forStaging);
static void freeGPUState(Simulator& simulator, Simulator::GPUState& state);

static Result<void> copyStagingToCPU(Simulator& simulator);
static Result<void> copyCPUToStaging(Simulator& simulator);

Result<void> initSimulator(Simulator* simulator, bool withDebug)
{
	CM_PROPAGATE_ERROR(initGPUContext(&simulator->gpuContext, withDebug));
	CM_PROPAGATE_ERROR(initGPUDevice(&simulator->gpuDevice, simulator->gpuContext));

	VkDevice deviceHandle = simulator->gpuDevice.device;

	//Create fence for waiting on queue submissions
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	VK_THROW(vkCreateFence(deviceHandle, &fenceCI, nullptr, &simulator->submitFinishedFence));

	//Create timing query pool
	VkQueryPoolCreateInfo queryPoolCI = {};
	queryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolCI.queryCount = 2;
	queryPoolCI.pipelineStatistics = 0;
	
	VK_THROW(vkCreateQueryPool(deviceHandle, &queryPoolCI, nullptr, &simulator->timingQueryPool));

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
	CM_TRY(simulator->gpuState, allocateNewGPUState(*simulator, simulator->cellCapacity, false));
	CM_TRY(simulator->stagingState, allocateNewGPUState(*simulator, simulator->cellCapacity, true));

	for (int i = 0; i < 11; ++i)
	{
		simulator->cpuState.positions[i] = { 0.f, 5.0f, 3.5f * (5 - i) }; //{ 0.f, 0.0f, 2.6f * (5 - i) }
		simulator->cpuState.rotations[i] = { 3.14159265359f / 2.0f, 0.0f };
		simulator->cpuState.sizes[i] = { 0.0f, 0.5f };
		simulator->cpuState.colors[i] = 0xFF0000FF;
	}

	simulator->cellCount = 11;

	simulator->uploadStateOnNextStep = true;

	return Result<void>();
}

Result<void> importShaders(Simulator& simulator, ShaderImportCallback importCallback)
{
	//Create a single descriptor pool for all the shaders
	VkDevice deviceHandle = simulator.gpuDevice.device;

	VkDescriptorPoolSize poolSizes[1] = {};
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 };

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
	params.descriptorBindings.push_back({ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });

	params.pushConstans.push_back({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GlobalConsts) });

	CM_TRY(auto& compiled, compileShaderSpirV(importCallback("shaders/collision_shader.glsl"), "shaders/collision_shader.glsl"));
	CM_TRY(simulator.collisionShader, createShaderPipeline(simulator.gpuDevice, compiled, params));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = simulator.descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &simulator.collisionShader.descSetLayout;

	VK_THROW(vkAllocateDescriptorSets(deviceHandle, &allocInfo, &simulator.stateDescSet));

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

	if (simulator.timingQueryPool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(deviceHandle, simulator.timingQueryPool, nullptr);
	}

	freeCPUState(simulator.cpuState);
	freeGPUState(simulator, simulator.gpuState);
	freeGPUState(simulator, simulator.stagingState);

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

	GlobalConsts consts = {};
	consts.cellCount = simulator.cellCount;
	consts.deltaTime = 0.01f;

	VK_THROW(vkResetCommandPool(device.device, device.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	///////////////////////////////////////////////////////////////
	// Update descriptor sets
	///////////////////////////////////////////////////////////////
	VkDescriptorBufferInfo bufferWriteInfos[4] = {};
	bufferWriteInfos[0].buffer = simulator.gpuState.positions.buffer;
	bufferWriteInfos[0].offset = 0;
	bufferWriteInfos[0].range = simulator.cellCount * sizeof(vec3);

	bufferWriteInfos[1].buffer = simulator.gpuState.rotations.buffer;
	bufferWriteInfos[1].offset = 0;
	bufferWriteInfos[1].range = simulator.cellCount * sizeof(vec2);

	bufferWriteInfos[2].buffer = simulator.gpuState.sizes.buffer;
	bufferWriteInfos[2].offset = 0;
	bufferWriteInfos[2].range = simulator.cellCount * sizeof(vec2);

	bufferWriteInfos[3].buffer = simulator.gpuState.velocities.buffer;
	bufferWriteInfos[3].offset = 0;
	bufferWriteInfos[3].range = simulator.cellCount * sizeof(vec3);

	VkWriteDescriptorSet descSetWrites[4] = {};

	for (int i = 0; i < 4; ++i)
	{
		descSetWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descSetWrites[i].dstSet = simulator.stateDescSet;
		descSetWrites[i].dstBinding = i;
		descSetWrites[i].dstArrayElement = 0;
		descSetWrites[i].descriptorCount = 1;
		descSetWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descSetWrites[i].pBufferInfo = &bufferWriteInfos[i];
	}

	vkUpdateDescriptorSets(device.device, sizeof(descSetWrites) / sizeof(descSetWrites[0]), descSetWrites, 0, nullptr);

	///////////////////////////////////////////////////////////////
	// Perform a step
	///////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_THROW(vkBeginCommandBuffer(device.commandBuffer, &beginInfo));

	//Write first timestamp
	vkCmdResetQueryPool(device.commandBuffer, simulator.timingQueryPool, 0, 2);
	vkCmdWriteTimestamp(device.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, simulator.timingQueryPool, 0);

	//Copy state to GPU
	VkBufferCopy copyRegions[] = {
		/* positions */
		{ 0, 0, simulator.cellCount * sizeof(vec3) },
		/* rotations */
		{ 0, 0, simulator.cellCount * sizeof(vec2) },
		/* sizes */
		{ 0, 0, simulator.cellCount * sizeof(vec2) },
		/* velocities */
		{ 0, 0, simulator.cellCount * sizeof(vec3) },
	};

	if (simulator.uploadStateOnNextStep)
	{
		copyCPUToStaging(simulator);

		vkCmdCopyBuffer(device.commandBuffer, simulator.stagingState.positions.buffer, simulator.gpuState.positions.buffer, 1, &copyRegions[0]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.stagingState.rotations.buffer, simulator.gpuState.rotations.buffer, 1, &copyRegions[1]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.stagingState.sizes.buffer, simulator.gpuState.sizes.buffer, 1, &copyRegions[2]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.stagingState.velocities.buffer, simulator.gpuState.velocities.buffer, 1, &copyRegions[3]);

		VkMemoryBarrier memoryBarrier = {};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(device.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
							 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		simulator.uploadStateOnNextStep = false;
	}

	// Run collision detection and accumulate forces
	vkCmdBindPipeline(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipeline);
	vkCmdBindDescriptorSets(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipelineLayout, 0, 1, &simulator.stateDescSet, 0, nullptr);
	
	vkCmdPushConstants(device.commandBuffer, simulator.collisionShader.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GlobalConsts), &consts);

	vkCmdDispatch(device.commandBuffer, workgroupCount(simulator.cellCount, 64), 1, 1);

	///////////////////////////////////////////////////////////////
	// Copy results to CPU
	///////////////////////////////////////////////////////////////
	VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(device.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						 1, &memoryBarrier, 0, nullptr, 0, nullptr);

	vkCmdCopyBuffer(device.commandBuffer, simulator.gpuState.positions.buffer, simulator.stagingState.positions.buffer, 1, &copyRegions[0]);
	vkCmdCopyBuffer(device.commandBuffer, simulator.gpuState.rotations.buffer, simulator.stagingState.rotations.buffer, 1, &copyRegions[1]);
	vkCmdCopyBuffer(device.commandBuffer, simulator.gpuState.sizes.buffer, simulator.stagingState.sizes.buffer, 1, &copyRegions[2]);
	vkCmdCopyBuffer(device.commandBuffer, simulator.gpuState.velocities.buffer, simulator.stagingState.velocities.buffer, 1, &copyRegions[3]);

	//Write second timestamp
	vkCmdWriteTimestamp(device.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, simulator.timingQueryPool, 1);

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

	if (simulator.queueWaitBegin) simulator.queueWaitBegin();

	VK_THROW(vkWaitForFences(device.device, 1, &simulator.submitFinishedFence, VK_TRUE, UINT64_MAX));
	VK_THROW(vkResetFences(device.device, 1, &simulator.submitFinishedFence));

	if (simulator.queueWaitEnd) simulator.queueWaitEnd();

	///////////////////////////////////////////////////////////////
	// Copy staging to CPU state (probably)
	///////////////////////////////////////////////////////////////
	// Note(Jason): When copying results of very large simulations, this can become a pretty
	// big bottleneck. We may want to consider copying the data only occasionally or not at all
	copyStagingToCPU(simulator);

	//Calculate the time it took for the step to complete
	uint64_t timestamps[2];
	VK_CHECK(vkGetQueryPoolResults(device.device, simulator.timingQueryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(timestamps[0]), VK_QUERY_RESULT_64_BIT));

	const uint32_t timestampValidBits = simulator.gpuDevice.queueProperties.timestampValidBits;
	const double timestampPeriod = (double)simulator.gpuDevice.properties.limits.timestampPeriod;

	const uint64_t timestampMask = timestampValidBits >= (sizeof(uint64_t) * 8) ? ~((uint64_t)0) : ((uint64_t)1 << timestampValidBits) - (uint64_t)(1);
	const double startTimestamp = (timestamps[0] & timestampMask) * timestampPeriod;
	const double endTimestamp = (timestamps[1] & timestampMask) * timestampPeriod;

	simulator.lastStepTime = (endTimestamp - startTimestamp) / 1e9;

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
	state.velocities = new vec3[size];
	state.colors = new uint32_t[size];

	return state;
}

void freeCPUState(Simulator::CPUState& state)
{
	delete[] state.positions;
	delete[] state.rotations;
	delete[] state.sizes;
	delete[] state.velocities;
	delete[] state.colors;
}

Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size, bool forStaging)
{
	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (forStaging)
	{
		usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

	Simulator::GPUState state = {};
	CM_TRY(state.positions, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec3), usageFlags, memoryProperties));
	CM_TRY(state.rotations, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec2), usageFlags, memoryProperties));
	CM_TRY(state.sizes, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec2), usageFlags, memoryProperties));
	CM_TRY(state.velocities, createGPUBuffer(simulator.gpuDevice, size * sizeof(vec3), usageFlags, memoryProperties));

	return state;
}

void freeGPUState(Simulator& simulator, Simulator::GPUState& state)
{
	destroyGPUBuffer(simulator.gpuDevice, state.positions);
	destroyGPUBuffer(simulator.gpuDevice, state.rotations);
	destroyGPUBuffer(simulator.gpuDevice, state.sizes);
	destroyGPUBuffer(simulator.gpuDevice, state.velocities);
}

Result<void> copyStagingToCPU(Simulator& simulator)
{
	VkDevice deviceHandle = simulator.gpuDevice.device;
	Simulator::GPUState& gpuState = simulator.gpuState;
	Simulator::CPUState& cpuState = simulator.cpuState;

	auto copyMemory = [&](VkDeviceMemory source, void* dest, size_t copyAmount) -> Result<void>
	{
		void* mem = nullptr;
		VK_THROW(vkMapMemory(deviceHandle, source, 0, copyAmount, 0, &mem));

		memcpy(dest, mem, copyAmount);

		vkUnmapMemory(deviceHandle, source);

		return Result<void>();
	};

	CM_PROPAGATE_ERROR(copyMemory(simulator.stagingState.positions.memory, simulator.cpuState.positions, simulator.cellCount * sizeof(vec3)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.stagingState.rotations.memory, simulator.cpuState.rotations, simulator.cellCount * sizeof(vec2)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.stagingState.sizes.memory, simulator.cpuState.sizes, simulator.cellCount * sizeof(vec2)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.stagingState.velocities.memory, simulator.cpuState.velocities, simulator.cellCount * sizeof(vec3)));

	return Result<void>();
}

Result<void> copyCPUToStaging(Simulator& simulator)
{
	VkDevice deviceHandle = simulator.gpuDevice.device;
	Simulator::GPUState& gpuState = simulator.gpuState;
	Simulator::CPUState& cpuState = simulator.cpuState;

	auto copyMemory = [&](void* source, VkDeviceMemory dest, size_t copyAmount) -> Result<void>
	{
		void* mem = nullptr;
		VK_THROW(vkMapMemory(deviceHandle, dest, 0, copyAmount, 0, &mem));

		memcpy(mem, source, copyAmount);

		vkUnmapMemory(deviceHandle, dest);

		return Result<void>();
	};

	CM_PROPAGATE_ERROR(copyMemory(simulator.cpuState.positions, simulator.stagingState.positions.memory, simulator.cellCount * sizeof(vec3)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.cpuState.rotations, simulator.stagingState.rotations.memory, simulator.cellCount * sizeof(vec2)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.cpuState.sizes, simulator.stagingState.sizes.memory, simulator.cellCount * sizeof(vec2)));
	CM_PROPAGATE_ERROR(copyMemory(simulator.cpuState.velocities, simulator.stagingState.velocities.memory, simulator.cellCount * sizeof(vec3)));

	return Result<void>();
}