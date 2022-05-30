#pragma once

#include "result.h"
#include "shader_compiler.h"

#include <volk.h>

#include <vector>

struct GPUContext
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

struct GPUDevice
{
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkPhysicalDeviceProperties properties = {};
	VkPhysicalDeviceFeatures features = {};
	VkPhysicalDeviceMemoryProperties memoryProperties = {};

	/*
	 In a big, complicated GPU application like a game, you'd normally
	 have multiple queues each with multiple command pools and you'd
	 need to allocate many command buffers. However, because this is a
	 much simpler application we can just use one queue, one command pool,
	 and one command buffer.
	*/
	VkQueue commandQueue = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	VkQueueFamilyProperties queueProperties = {};
	uint32_t queueFamilyIndex = 0xFFFFFFFFu;
};

struct ShaderPipeline
{
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	//Normaly, you'd want to have multiple descriptor sets per pipeline.
	//However, in this case, most shaders will probably be fine with just
	//using one descriptor set.
	//VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
};

struct PipelineParameters
{
	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<VkPushConstantRange> pushConstans;
};

struct GPUBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint64_t size = 0;
};

Result<void> initGPUContext(GPUContext* context, bool withDebug = false);
void deinitGPUContext(GPUContext& context);

Result<void> initGPUDevice(GPUDevice* device, GPUContext& context);
void deinitGPUDevice(GPUDevice& device);

Result<GPUBuffer> createGPUBuffer(GPUDevice& device, uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProperties);
void destroyGPUBuffer(GPUDevice& device, GPUBuffer& buffer);

Result<ShaderPipeline> createShaderPipeline(GPUDevice& device, const CompiledShader& compiledShader, const PipelineParameters& params);
void destroyShaderPipeline(GPUDevice& device, const ShaderPipeline& shader);

Result<void> checkVk(VkResult result, const char* file, unsigned int line);

#define VK_CHECK_SAFE(x) ((x) == VK_SUCCESS)
#define VK_CHECK(x) checkVk(x, __FILE__, __LINE__)
#define VK_THROW(x) CM_PROPAGATE_ERROR(checkVk(x, __FILE__, __LINE__))