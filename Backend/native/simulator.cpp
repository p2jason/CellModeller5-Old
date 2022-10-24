#include "simulator.h"

#include "shader_compiler.h"
#include "frame_capture.h"

#include "zlib.h"

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

static Result<void> mapGPUState(Simulator& simulator);
static void unmapGPUState(Simulator& simulator);

static Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size, bool onHost);
static void freeGPUState(Simulator& simulator, Simulator::GPUState& state);

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

	CM_TRY(simulator->cpuStateMemory, allocateNewGPUState(*simulator, simulator->cellCapacity, true));
	CM_TRY(simulator->gpuStates[0], allocateNewGPUState(*simulator, simulator->cellCapacity, false));
	CM_TRY(simulator->gpuStates[1], allocateNewGPUState(*simulator, simulator->cellCapacity, false));

	CM_PROPAGATE_ERROR(mapGPUState(*simulator));

#if 0
	/*for (int i = 0; i < 11; ++i)
	{
		simulator->cpuState.positions[i] = { 0.f, 5.0f, 3.5f * (5 - i) }; //{ 0.f, 0.0f, 2.6f * (5 - i) }
		simulator->cpuState.rotations[i] = { 3.14159265359f / 2.0f, 0.0f };
		simulator->cpuState.sizes[i] = { 0.0f, 0.5f };
		//simulator->cpuState.colors[i] = 0xFF0000FF;
	}

	simulator->cellCount = 11;*/
#else
	simulator->cpuState.positions[0] = { 0.f, 2.0f, 0.0f };
	simulator->cpuState.rotations[0] = { 0.0f, 0.0f };
	simulator->cpuState.velocities[0] = { 0.0f, 2.0f, 0.0f };
	simulator->cpuState.sizes[0] = { 0.0f, 1.0f };

	simulator->cpuState.positions[1] = { 0.f, 8.0f, 0.0f };
	simulator->cpuState.rotations[1] = { 0.0f, 0.0f };
	simulator->cpuState.velocities[1] = { 0.0f, -2.0f, 0.0f };
	simulator->cpuState.sizes[1] = { 0.0f, 1.0f };

	simulator->cellCount = 2;
#endif

	simulator->uploadStateOnNextStep = true;
	simulator->compressionLevel = 2;

	//Initialize frame capture
	initFrameCapture();

	return Result<void>();
}

Result<void> importShaders(Simulator& simulator, ShaderImportCallback importCallback)
{
	//Create a single descriptor pool for all the shaders
	VkDevice deviceHandle = simulator.gpuDevice.device;

	VkDescriptorPoolSize poolSizes[1] = {};
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8 };

	VkDescriptorPoolCreateInfo poolCI = {};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 2;
	poolCI.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
	poolCI.pPoolSizes = poolSizes;

	VK_THROW(vkCreateDescriptorPool(deviceHandle, &poolCI, nullptr, &simulator.descriptorPool));

	/*********** State descriptor set ***********/
	{
		VkDescriptorSetLayoutBinding layoutBindings[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
			{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		};

		VkDescriptorSetLayoutCreateInfo descSetLayoutCI = {};
		descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCI.bindingCount = (uint32_t)(sizeof(layoutBindings) / sizeof(layoutBindings[0]));
		descSetLayoutCI.pBindings = layoutBindings;

		VK_THROW(vkCreateDescriptorSetLayout(deviceHandle, &descSetLayoutCI, nullptr, &simulator.stateDescLayout));

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = simulator.descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &simulator.stateDescLayout;

		VK_THROW(vkAllocateDescriptorSets(deviceHandle, &allocInfo, &simulator.inputStateDescSet));
		VK_THROW(vkAllocateDescriptorSets(deviceHandle, &allocInfo, &simulator.outputStateDescSet));
	}

	/*********** Collision detection shader ***********/
	PipelineParameters params = {};
	params.descSetLayouts.push_back(simulator.stateDescLayout);
	params.descSetLayouts.push_back(simulator.stateDescLayout);

	params.pushConstans.push_back({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GlobalConsts) });

	CM_TRY(auto& compiled, compileShaderSpirV(importCallback("shaders/collision_shader.glsl"), "shaders/collision_shader.glsl"));
	CM_TRY(simulator.collisionShader, createShaderPipeline(simulator.gpuDevice, compiled, params));

	return Result<void>();
}

static void destroyShaders(Simulator& simulator)
{
	VkDevice deviceHandle = simulator.gpuDevice.device;

	if (simulator.descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(deviceHandle, simulator.descriptorPool, nullptr);
	}

	if (simulator.stateDescLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(deviceHandle, simulator.stateDescLayout, nullptr);
	}

	destroyShaderPipeline(simulator.gpuDevice, simulator.collisionShader);
}

void deinitSimulator(Simulator& simulator)
{
	VkDevice deviceHandle = simulator.gpuDevice.device;

	destroyShaders(simulator);

	if (simulator.submitFinishedFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(deviceHandle, simulator.submitFinishedFence, nullptr);
	}

	if (simulator.timingQueryPool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(deviceHandle, simulator.timingQueryPool, nullptr);
	}

	unmapGPUState(simulator);

	freeGPUState(simulator, simulator.cpuStateMemory);
	freeGPUState(simulator, simulator.gpuStates[0]);
	freeGPUState(simulator, simulator.gpuStates[1]);

	deinitGPUDevice(simulator.gpuDevice);
	deinitGPUContext(simulator.gpuContext);
}

static uint32_t workgroupCount(uint32_t threadCount, uint32_t groupSize)
{
	uint32_t m = threadCount % groupSize;
	return (m ? (threadCount + groupSize - m) : threadCount) / groupSize;
}

static void updateStateSet(Simulator& simulator, Simulator::GPUState& state, VkDescriptorSet descSet)
{
	VkDescriptorBufferInfo bufferWrites[8] = {};
	bufferWrites[0].buffer = state.positions.buffer;
	bufferWrites[0].range = simulator.cellCount * sizeof(vec3);

	bufferWrites[1].buffer = state.rotations.buffer;
	bufferWrites[1].range = simulator.cellCount * sizeof(vec2);

	bufferWrites[2].buffer = state.sizes.buffer;
	bufferWrites[2].range = simulator.cellCount * sizeof(vec2);

	bufferWrites[3].buffer = state.velocities.buffer;
	bufferWrites[3].range = simulator.cellCount * sizeof(vec3);

	VkWriteDescriptorSet descSetWrites[4] = {};

	for (int i = 0; i < 4; ++i)
	{
		descSetWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descSetWrites[i].dstSet = descSet;
		descSetWrites[i].dstBinding = i;
		descSetWrites[i].dstArrayElement = 0;
		descSetWrites[i].descriptorCount = 1;
		descSetWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descSetWrites[i].pBufferInfo = &bufferWrites[i];
	}

	vkUpdateDescriptorSets(simulator.gpuDevice.device, sizeof(descSetWrites) / sizeof(descSetWrites[0]), descSetWrites, 0, nullptr);
}

Result<void> stepSimulator(Simulator& simulator)
{
	//This is used to automatically start/stop frame capture
	FrameCaptureScope frameCapture;

	GPUDevice& device = simulator.gpuDevice;

	Simulator::GPUState& inputState = simulator.gpuStates[simulator.gpuStateToggle ? 0 : 1];
	Simulator::GPUState& outputState = simulator.gpuStates[simulator.gpuStateToggle ? 1 : 0];

	simulator.gpuStateToggle = !simulator.gpuStateToggle;

	GlobalConsts consts = {};
	consts.cellCount = simulator.cellCount;
	consts.deltaTime = 0.03f;

	VK_THROW(vkResetCommandPool(device.device, device.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

	///////////////////////////////////////////////////////////////
	// Update descriptor sets
	///////////////////////////////////////////////////////////////
	updateStateSet(simulator, inputState, simulator.inputStateDescSet);
	updateStateSet(simulator, outputState, simulator.outputStateDescSet);

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
		//Copy the CPU state to the input state on the GPU
		vkCmdCopyBuffer(device.commandBuffer, simulator.cpuStateMemory.positions.buffer, inputState.positions.buffer, 1, &copyRegions[0]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.cpuStateMemory.rotations.buffer, inputState.rotations.buffer, 1, &copyRegions[1]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.cpuStateMemory.sizes.buffer, inputState.sizes.buffer, 1, &copyRegions[2]);
		vkCmdCopyBuffer(device.commandBuffer, simulator.cpuStateMemory.velocities.buffer, inputState.velocities.buffer, 1, &copyRegions[3]);

		VkMemoryBarrier memoryBarrier = {};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(device.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
							 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		//Copy the input state to the output state. This is done just in case some shader doesn't write to the output state.
		vkCmdCopyBuffer(device.commandBuffer, inputState.positions.buffer, outputState.positions.buffer, 1, &copyRegions[0]);
		vkCmdCopyBuffer(device.commandBuffer, inputState.rotations.buffer, outputState.rotations.buffer, 1, &copyRegions[1]);
		vkCmdCopyBuffer(device.commandBuffer, inputState.sizes.buffer, outputState.sizes.buffer, 1, &copyRegions[2]);
		vkCmdCopyBuffer(device.commandBuffer, inputState.velocities.buffer, outputState.velocities.buffer, 1, &copyRegions[3]);

		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(device.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
							 1, &memoryBarrier, 0, nullptr, 0, nullptr);

		simulator.uploadStateOnNextStep = false;
	}

	// Run collision detection and accumulate forces
	vkCmdBindPipeline(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipeline);
	vkCmdBindDescriptorSets(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipelineLayout, 0, 1, &simulator.inputStateDescSet, 0, nullptr);
	vkCmdBindDescriptorSets(device.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, simulator.collisionShader.pipelineLayout, 1, 1, &simulator.outputStateDescSet, 0, nullptr);
	
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

	vkCmdCopyBuffer(device.commandBuffer, outputState.positions.buffer, simulator.cpuStateMemory.positions.buffer, 1, &copyRegions[0]);
	vkCmdCopyBuffer(device.commandBuffer, outputState.rotations.buffer, simulator.cpuStateMemory.rotations.buffer, 1, &copyRegions[1]);
	vkCmdCopyBuffer(device.commandBuffer, outputState.sizes.buffer, simulator.cpuStateMemory.sizes.buffer, 1, &copyRegions[2]);
	vkCmdCopyBuffer(device.commandBuffer, outputState.velocities.buffer, simulator.cpuStateMemory.velocities.buffer, 1, &copyRegions[3]);

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

	//Calculate the time it took for the step to complete
	uint64_t timestamps[2];
	VK_CHECK(vkGetQueryPoolResults(device.device, simulator.timingQueryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(timestamps[0]), VK_QUERY_RESULT_64_BIT));

	uint32_t timestampValidBits = simulator.gpuDevice.queueProperties.timestampValidBits;
	double timestampPeriod = (double)simulator.gpuDevice.properties.limits.timestampPeriod;

	uint64_t timestampMask = timestampValidBits >= (sizeof(uint64_t) * 8) ? ~((uint64_t)0) : ((uint64_t)1 << timestampValidBits) - (uint64_t)(1);
	double startTimestamp = (timestamps[0] & timestampMask) * timestampPeriod;
	double endTimestamp = (timestamps[1] & timestampMask) * timestampPeriod;

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

Result<void> compressToFile(uint8_t* inputBuffer, uint32_t inputSize, int level, std::string filepath)
{
	std::ofstream out(filepath, std::ios::binary | std::ios::trunc);

	if (!out)
	{
		CM_ERROR_MESSAGE("Failed to open viz file: " + filepath);
	}

	uint8_t outBuffer[4096];

	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	int ret = deflateInit(&stream, level);
	if (ret != Z_OK) CM_ERROR_MESSAGE("Failed to initialize deflate algorithm");

	stream.avail_in = inputSize;
	stream.next_in = inputBuffer;

	do
	{
		stream.avail_out = sizeof(outBuffer);
		stream.next_out = outBuffer;

		ret = deflate(&stream, Z_FINISH);
		if (ret == Z_STREAM_ERROR) CM_ERROR_MESSAGE("Error occured when compressing buffer");

		int available = sizeof(outBuffer) - stream.avail_out;

		try
		{
			out.write((const char*)outBuffer, available);
		}
		catch (...)
		{
			deflateEnd(&stream);
			CM_ERROR_MESSAGE("Error occured when writing to file: " + filepath);
		}
	} while (stream.avail_out == 0);

	deflateEnd(&stream);

	out.close();

	return Result<void>();
}

Result<void> writeSimulatorStateToVizFile(Simulator& simulator, std::string filepath)
{
	size_t elemWidth = 8 * sizeof(float) + sizeof(uint32_t);

	std::vector<uint8_t> buffer(sizeof(uint32_t) + simulator.cellCount * elemWidth + simulator.cellCount * sizeof(uint64_t));

	union
	{
		uint8_t* asByte;

		float* asFloat;
		uint32_t* asUInt;
		uint64_t* asULong;
	} alias;

	alias.asByte = buffer.data();
	*(alias.asUInt++) = simulator.cellCount;
	
	for (uint32_t i = 0; i < simulator.cellCount; ++i)
	{
		vec3 pos = simulator.cpuState.positions[i];
		vec3 dir = directionFromAngles(simulator.cpuState.rotations[i]);
		vec2 size = simulator.cpuState.sizes[i];

		//TODO: Correct byte order
		*(alias.asFloat++) = pos.x;
		*(alias.asFloat++) = pos.y;
		*(alias.asFloat++) = pos.z;

		*(alias.asFloat++) = dir.x;
		*(alias.asFloat++) = dir.y;
		*(alias.asFloat++) = dir.z;

		*(alias.asFloat++) = size.x;
		*(alias.asFloat++) = size.y;
		*(alias.asUInt++) = 0xFF0000FF;//simulator.cpuState.colors[i];
	}

	for (uint32_t i = 0; i < simulator.cellCount; ++i)
	{
		//TODO: Use correct cell id
		//TODO: Correct byte order
		*(alias.asULong++) = i;
	}

	compressToFile(buffer.data(), (uint32_t)buffer.size(), simulator.compressionLevel, filepath);

	return Result<void>();
}

Result<void> mapGPUState(Simulator& simulator)
{
	VkDevice device = simulator.gpuDevice.device;

	VK_THROW(vkMapMemory(device, simulator.cpuStateMemory.positions.memory, 0, simulator.cpuStateMemory.positions.size, 0, (void**)&simulator.cpuState.positions));
	VK_THROW(vkMapMemory(device, simulator.cpuStateMemory.rotations.memory, 0, simulator.cpuStateMemory.rotations.size, 0, (void**)&simulator.cpuState.rotations));
	VK_THROW(vkMapMemory(device, simulator.cpuStateMemory.sizes.memory, 0, simulator.cpuStateMemory.sizes.size, 0, (void**)&simulator.cpuState.sizes));
	VK_THROW(vkMapMemory(device, simulator.cpuStateMemory.velocities.memory, 0, simulator.cpuStateMemory.velocities.size, 0, (void**)&simulator.cpuState.velocities));

	return Result<void>();
}

void unmapGPUState(Simulator& simulator)
{
	VkDevice device = simulator.gpuDevice.device;

	vkUnmapMemory(device, simulator.cpuStateMemory.positions.memory);
	vkUnmapMemory(device, simulator.cpuStateMemory.rotations.memory);
	vkUnmapMemory(device, simulator.cpuStateMemory.sizes.memory);
	vkUnmapMemory(device, simulator.cpuStateMemory.velocities.memory);
}

Result<Simulator::GPUState> allocateNewGPUState(Simulator& simulator, uint32_t size, bool onHost)
{
	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (onHost)
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