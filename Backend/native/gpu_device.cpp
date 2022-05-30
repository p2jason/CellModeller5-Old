#include "gpu_device.h"

#include "renderdoc/renderdoc_app.h"

#include <string>
#include <vector>
#include <cassert>
#include <sstream>

static int minSeverity = 2;

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	bool shouldIgnore = false;

	// Check if the debug message should be ignored
	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		shouldIgnore |= strstr(pCallbackData->pMessage, "loaderAddLayerProperties") != nullptr;
	}

	if (shouldIgnore)
	{
		return VK_FALSE;
	}

	//Log debug message
	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		if (minSeverity <= 0)
		{
			printf("VulkanDebugCallback:\n %s\n", pCallbackData->pMessage);
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		if (minSeverity <= 1)
		{
			printf("VulkanDebugCallback:\n %s\n", pCallbackData->pMessage);
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		if (minSeverity <= 2)
		{
			printf("VulkanDebugCallback:\n %s\n", pCallbackData->pMessage);
		}
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		if (minSeverity <= 3)
		{
			printf("VulkanDebugCallback:\n %s\n", pCallbackData->pMessage);
		}
		break;
	}

	//Throw error
	assert(messageSeverity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);

	return VK_FALSE;
}

void queryValidationLayers(const std::vector<const char*>& validationLayers, std::vector<const char*>& enabledLayers)
{
	uint32_t availableLayerCount = 0;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));

	std::vector<VkLayerProperties> availableLayers(availableLayerCount);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data()));

	for (size_t i = 0; i < validationLayers.size(); ++i)
	{
		bool found = false;

		for (size_t j = 0; j < availableLayers.size(); ++j)
		{
			const char* layerName = availableLayers[j].layerName;

			if (strncmp(validationLayers[i], layerName, VK_MAX_EXTENSION_NAME_SIZE) == 0)
			{
				enabledLayers.push_back(validationLayers[i]);

				found = true;
				break;
			}
		}

		if (!found)
		{
			//TODO: Maybe print a warning
		}
	}
}

void queryInstanceExtensions(const std::vector<const char*>& instanceExtensions)
{
	uint32_t availableExtensionCount = 0;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr));

	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data()));

	for (size_t i = 0; i < instanceExtensions.size(); ++i)
	{
		bool found = false;

		for (size_t j = 0; j < availableExtensions.size(); ++j)
		{
			const char* extensionName = availableExtensions[j].extensionName;

			if (strncmp(instanceExtensions[i], extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			//Throw error
		}
	}
}

Result<void> initGPUContext(GPUContext* context, bool withDebug)
{
	if (!VK_CHECK_SAFE(volkInitialize()))
	{
		return CM_ERROR_MESSAGE("Could not find Vulkan libraries");
	}

	//Check for validation layer support
	std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> enabledLayers;

	if (withDebug)
	{
		queryValidationLayers(validationLayers, enabledLayers);
	}
	
	//Check for instance extension support
	std::vector<const char*> instanceExtensions = {};

	if (withDebug)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	queryInstanceExtensions(instanceExtensions);

	//Create instance
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.pNext = nullptr;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = VulkanDebugUtilsCallback;
	debugCreateInfo.pUserData = nullptr;

	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "Cell Modeller";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "Cell Modeller Engine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(5, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCI = {};
	instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCI.pNext = &debugCreateInfo;
	instanceCI.pApplicationInfo = &applicationInfo;
	instanceCI.enabledExtensionCount = (uint32_t)instanceExtensions.size();
	instanceCI.ppEnabledExtensionNames = instanceExtensions.data();
	instanceCI.enabledLayerCount = (uint32_t)enabledLayers.size();
	instanceCI.ppEnabledLayerNames = withDebug ? enabledLayers.data() : nullptr;

	VK_THROW(vkCreateInstance(&instanceCI, nullptr, &context->instance));

	volkLoadInstance(context->instance);

	if (withDebug)
	{
		VK_THROW(vkCreateDebugUtilsMessengerEXT(context->instance, &debugCreateInfo, nullptr, &context->debugMessenger));
	}

	return Result<void>();
}

void deinitGPUContext(GPUContext& context)
{
	if (context.debugMessenger)
	{
		vkDestroyDebugUtilsMessengerEXT(context.instance, context.debugMessenger, nullptr);
	}

	vkDestroyInstance(context.instance, nullptr);
}

Result<void> initGPUDevice(GPUDevice* device, GPUContext& context)
{
	//Select physical device
	uint32_t physicalDeviceCount = 0;
	VK_THROW(vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount, nullptr));

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	VK_THROW(vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount, physicalDevices.data()));

	uint32_t selectedDevice = 0xFFFFFFFFu;

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties props = {};
		vkGetPhysicalDeviceProperties(physicalDevices[i], &props);
		
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			selectedDevice = i;
			break;
		}
		else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
		{
			selectedDevice = i;
		}
	}

	if (selectedDevice == 0xFFFFFFFFu)
	{
		return CM_ERROR_MESSAGE("Failed to find a suitable GPU");
	}

	//Get physical device properties
	device->physicalDevice = physicalDevices[selectedDevice];

	vkGetPhysicalDeviceProperties(device->physicalDevice, &device->properties);
	vkGetPhysicalDeviceFeatures(device->physicalDevice, &device->features);
	vkGetPhysicalDeviceMemoryProperties(device->physicalDevice, &device->memoryProperties);

	//Select command queue
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, queueProperties.data());

	uint32_t selectedQueue = 0xFFFFFFFFu;

	for (uint32_t i = 0; i < queueCount; ++i)
	{
		VkQueueFlags flags = queueProperties[i].queueFlags;

		if ((flags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
		{
			selectedQueue = i;
			break;
		}
	}

	if (selectedQueue == 0xFFFFFFFFu)
	{
		return CM_ERROR_MESSAGE("Failed to find a command queue with compute support");
	}

	device->queueProperties = queueProperties[selectedQueue];
	device->queueFamilyIndex = selectedQueue;

	//Create device
	float queuePriority = 1.0;

	VkDeviceQueueCreateInfo queueCI = {};
	queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCI.queueFamilyIndex = selectedDevice;
	queueCI.queueCount = 1;
	queueCI.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceCI = {};
	deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.queueCreateInfoCount = 1;
	deviceCI.pQueueCreateInfos = &queueCI;
	deviceCI.enabledLayerCount = 0;
	deviceCI.ppEnabledLayerNames = nullptr;
	deviceCI.enabledExtensionCount = 0;
	deviceCI.ppEnabledExtensionNames = nullptr;
	deviceCI.pEnabledFeatures = nullptr;
	
	VK_THROW(vkCreateDevice(device->physicalDevice, &deviceCI, nullptr, &device->device));

	volkLoadDevice(device->device);

	//Create command queue
	vkGetDeviceQueue(device->device, selectedQueue, 0, &device->commandQueue);

	VkCommandPoolCreateInfo commandPoolCI = {};
	commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.queueFamilyIndex = selectedQueue;
	commandPoolCI.flags = 0;

	VK_THROW(vkCreateCommandPool(device->device, &commandPoolCI, nullptr, &device->commandPool));

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = device->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VK_THROW(vkAllocateCommandBuffers(device->device, &allocInfo, &device->commandBuffer));

	return Result<void>();
}

void deinitGPUDevice(GPUDevice& device)
{
	vkFreeCommandBuffers(device.device, device.commandPool, 1, &device.commandBuffer);
	vkDestroyCommandPool(device.device, device.commandPool, nullptr);

	vkDestroyDevice(device.device, nullptr);
}

static Result<uint32_t> findMemoryTypeIndex(GPUDevice& device, uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	const VkPhysicalDeviceMemoryProperties& memProperties = device.memoryProperties;

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if (typeBits & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return Result<uint32_t>(i);
		}
	}

	return CM_ERROR_MESSAGE("Could not find appropriate memory type");
}

Result<GPUBuffer> createGPUBuffer(GPUDevice& device, uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProperties)
{
	GPUBuffer buffer = {};
	buffer.size = size;

	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	VK_THROW(vkCreateBuffer(device.device, &createInfo, nullptr, &buffer.buffer));

	VkMemoryRequirements requirements = {};
	vkGetBufferMemoryRequirements(device.device, buffer.buffer, &requirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = requirements.size;	
	CM_TRY(allocateInfo.memoryTypeIndex, findMemoryTypeIndex(device, requirements.memoryTypeBits, memProperties));

	VK_THROW(vkAllocateMemory(device.device, &allocateInfo, nullptr, &buffer.memory));
	VK_THROW(vkBindBufferMemory(device.device, buffer.buffer, buffer.memory, 0));

	return Result<GPUBuffer>(buffer);
}

void destroyGPUBuffer(GPUDevice& device, GPUBuffer& buffer)
{
	if (buffer.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device.device, buffer.buffer, nullptr);
	if (buffer.memory != VK_NULL_HANDLE) vkFreeMemory(device.device, buffer.memory, nullptr);
}

Result<ShaderPipeline> createShaderPipeline(GPUDevice& device, const CompiledShader& compiledShader, const PipelineParameters& params)
{
	ShaderPipeline pipeline;

	VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = (uint32_t)params.descSetLayouts.size();
	pipelineLayoutCI.pSetLayouts = params.descSetLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = (uint32_t)params.pushConstans.size();
	pipelineLayoutCI.pPushConstantRanges = params.pushConstans.data();

	VK_THROW(vkCreatePipelineLayout(device.device, &pipelineLayoutCI, nullptr, &pipeline.pipelineLayout));

	VkShaderModuleCreateInfo shaderModuleCI = {};
	shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCI.codeSize = compiledShader.size() * sizeof(compiledShader[0]);
	shaderModuleCI.pCode = compiledShader.data();

	VkShaderModule module;
	VK_THROW(vkCreateShaderModule(device.device, &shaderModuleCI, nullptr, &module));

	VkComputePipelineCreateInfo pipelineCI = {};
	pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCI.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineCI.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineCI.stage.module = module;
	pipelineCI.stage.pName = "main";
	pipelineCI.stage.pSpecializationInfo = nullptr;
	pipelineCI.layout = pipeline.pipelineLayout;
	
	VK_THROW(vkCreateComputePipelines(device.device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline.pipeline));

	//We only need the shader module to create the pipeline, so we
	//can destroy it once the pipeline has been created
	vkDestroyShaderModule(device.device, module, nullptr);

	return pipeline;
}

void destroyShaderPipeline(GPUDevice& device, const ShaderPipeline& shader)
{
	vkDestroyPipeline(device.device, shader.pipeline, nullptr);
	vkDestroyPipelineLayout(device.device, shader.pipelineLayout, nullptr);
}

static const char* convertVkResultToString(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
	default: return "Unknown VkResult";
	}
}

Result<void> checkVk(VkResult result, const char* file, unsigned int line)
{
	if (!VK_CHECK_SAFE(result))
	{
		std::stringstream ss;
		ss << "Vulkan error: ";
		ss << convertVkResultToString(result);
		ss << " (" << file << ":" << line << ")";

		return CM_ERROR_MESSAGE(ss.str());
	}

	return Result<void>();
}