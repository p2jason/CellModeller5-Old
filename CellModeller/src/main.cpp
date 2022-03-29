#include <iostream>
#include <variant>

#include "result.h"
#include "gpu_device.h"
#include "shader_compiler.h"

const char* testString = R"Boo(#version 450

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform Config {
	mat4 transform;
	int matrixCount;
} opData;

layout(set = 0, binding = 1) readonly buffer InputBuffer {
	mat4 matrices[];
} sourceData;

layout(set = 0, binding = 2) buffer OutputBuffer{
	mat4 matrices[];
} outputData;

void main() {
	uint gID = gl_GlobalInvocationID.x;

	if(gID < opData.matrixCount) {
		outputData.matrices[gID] = sourceData.matrices[gID] * opData.transform;
	}
})Boo";

int main()
{
	GPUContext context = {};
	if (CM_IS_RESULT_FAILURE(initGPUContext(&context, true)))
	{
		return -1;
	}

	GPUDevice device = {};
	if (CM_IS_RESULT_FAILURE(initGPUDevice(&device, context)))
	{
		return -1;
	}

	startupShaderCompiler();

	printf("Name: %s\n", (const char*)device.properties.deviceName);

	auto shader = compileShaderSpirV(testString);

	if (CM_IS_RESULT_FAILURE(shader))
	{
		printf("%s\n", CM_RESULT_ERROR(shader).message.c_str());
		std::cin.get();
	}

	PipelineParameters params = {};
	params.descriptorBindings.push_back({ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
	params.descriptorBindings.push_back({ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
	params.descriptorBindings.push_back({ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });

	auto shaderPipeline = createShaderPipeline(device, CM_RESULT_VALUE(shader), params);

	if (CM_IS_RESULT_FAILURE(shaderPipeline))
	{
		printf("%s\n", CM_RESULT_ERROR(shaderPipeline).message.c_str());
		std::cin.get();
	}

	std::cout << "All good\n";

	destroyShaderPipeline(device, CM_RESULT_VALUE(shaderPipeline));
	
	terminateShaderCompiler();

	deinitGPUDevice(device);
	deinitGPUContext(context);

	std::cout << "Yeahy!";

	return 0;
}