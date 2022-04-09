#include "simulator.h"

//const COLLISION_DETECTION_SHADER = 

Result<void> initSimulator(Simulator* simulator, bool withDebug)
{
	CM_PROPAGATE_ERROR(initGPUContext(&simulator->gpuContext, withDebug));
	CM_PROPAGATE_ERROR(initGPUDevice(&simulator->gpuDevice, simulator->gpuContext));

	

	//Print details
	const VkPhysicalDeviceProperties& properties = simulator->gpuDevice.properties;

	printf("Vulkan Device:\n");
	printf("    Name:  %s\n", properties.deviceName);
	printf("    API Version: %d.%d.%d\n", VK_API_VERSION_MAJOR(properties.apiVersion),
										  VK_API_VERSION_MINOR(properties.apiVersion),
										  VK_API_VERSION_PATCH(properties.apiVersion));

	return Result<void>();
}

void deinitSimulator(Simulator& simulator)
{
	deinitGPUDevice(simulator.gpuDevice);
	deinitGPUContext(simulator.gpuContext);
}