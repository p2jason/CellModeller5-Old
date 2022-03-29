#include "shader_compiler.h"

#include <memory>
#include <sstream>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/Logger.h>

//#include <spirv_glsl.hpp>

const TBuiltInResource DefaultTBuiltInResource =
{
	/* .maxLights = */ 32,
	/* .maxClipPlanes = */ 6,
	/* .maxTextureUnits = */ 32,
	/* .maxTextureCoords = */ 32,
	/* .maxVertexAttribs = */ 64,
	/* .maxVertexUniformComponents = */ 4096,
	/* .maxVaryingFloats = */ 64,
	/* .maxVertexTextureImageUnits = */ 32,
	/* .maxCombinedTextureImageUnits = */ 80,
	/* .maxTextureImageUnits = */ 32,
	/* .maxFragmentUniformComponents = */ 4096,
	/* .maxDrawBuffers = */ 32,
	/* .maxVertexUniformVectors = */ 128,
	/* .maxVaryingVectors = */ 8,
	/* .maxFragmentUniformVectors = */ 16,
	/* .maxVertexOutputVectors = */ 16,
	/* .maxFragmentInputVectors = */ 15,
	/* .minProgramTexelOffset = */ -8,
	/* .maxProgramTexelOffset = */ 7,
	/* .maxClipDistances = */ 8,
	/* .maxComputeWorkGroupCountX = */ 65535,
	/* .maxComputeWorkGroupCountY = */ 65535,
	/* .maxComputeWorkGroupCountZ = */ 65535,
	/* .maxComputeWorkGroupSizeX = */ 1024,
	/* .maxComputeWorkGroupSizeY = */ 1024,
	/* .maxComputeWorkGroupSizeZ = */ 64,
	/* .maxComputeUniformComponents = */ 1024,
	/* .maxComputeTextureImageUnits = */ 16,
	/* .maxComputeImageUniforms = */ 8,
	/* .maxComputeAtomicCounters = */ 8,
	/* .maxComputeAtomicCounterBuffers = */ 1,
	/* .maxVaryingComponents = */ 60,
	/* .maxVertexOutputComponents = */ 64,
	/* .maxGeometryInputComponents = */ 64,
	/* .maxGeometryOutputComponents = */ 128,
	/* .maxFragmentInputComponents = */ 128,
	/* .maxImageUnits = */ 8,
	/* .maxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .maxCombinedShaderOutputResources = */ 8,
	/* .maxImageSamples = */ 0,
	/* .maxVertexImageUniforms = */ 0,
	/* .maxTessControlImageUniforms = */ 0,
	/* .maxTessEvaluationImageUniforms = */ 0,
	/* .maxGeometryImageUniforms = */ 0,
	/* .maxFragmentImageUniforms = */ 8,
	/* .maxCombinedImageUniforms = */ 8,
	/* .maxGeometryTextureImageUnits = */ 16,
	/* .maxGeometryOutputVertices = */ 256,
	/* .maxGeometryTotalOutputComponents = */ 1024,
	/* .maxGeometryUniformComponents = */ 1024,
	/* .maxGeometryVaryingComponents = */ 64,
	/* .maxTessControlInputComponents = */ 128,
	/* .maxTessControlOutputComponents = */ 128,
	/* .maxTessControlTextureImageUnits = */ 16,
	/* .maxTessControlUniformComponents = */ 1024,
	/* .maxTessControlTotalOutputComponents = */ 4096,
	/* .maxTessEvaluationInputComponents = */ 128,
	/* .maxTessEvaluationOutputComponents = */ 128,
	/* .maxTessEvaluationTextureImageUnits = */ 16,
	/* .maxTessEvaluationUniformComponents = */ 1024,
	/* .maxTessPatchComponents = */ 120,
	/* .maxPatchVertices = */ 32,
	/* .maxTessGenLevel = */ 64,
	/* .maxViewports = */ 16,
	/* .maxVertexAtomicCounters = */ 0,
	/* .maxTessControlAtomicCounters = */ 0,
	/* .maxTessEvaluationAtomicCounters = */ 0,
	/* .maxGeometryAtomicCounters = */ 0,
	/* .maxFragmentAtomicCounters = */ 8,
	/* .maxCombinedAtomicCounters = */ 8,
	/* .maxAtomicCounterBindings = */ 1,
	/* .maxVertexAtomicCounterBuffers = */ 0,
	/* .maxTessControlAtomicCounterBuffers = */ 0,
	/* .maxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .maxGeometryAtomicCounterBuffers = */ 0,
	/* .maxFragmentAtomicCounterBuffers = */ 1,
	/* .maxCombinedAtomicCounterBuffers = */ 1,
	/* .maxAtomicCounterBufferSize = */ 16384,
	/* .maxTransformFeedbackBuffers = */ 4,
	/* .maxTransformFeedbackInterleavedComponents = */ 64,
	/* .maxCullDistances = */ 8,
	/* .maxCombinedClipAndCullDistances = */ 8,
	/* .maxSamples = */ 4,
	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,
	/* .maxDualSourceDrawBuffersEXT = */ 1,

	/* .limits = */
	{
		/* .nonInductiveForLoops = */ true,
		/* .whileLoops = */ true,
		/* .doWhileLoops = */ true,
		/* .generalUniformIndexing = */ true,
		/* .generalAttributeMatrixVectorIndexing = */ true,
		/* .generalVaryingIndexing = */ true,
		/* .generalSamplerIndexing = */ true,
		/* .generalVariableIndexing = */ true,
		/* .generalConstantMatrixVectorIndexing = */ true
	}
};

bool startupShaderCompiler()
{
	return glslang::InitializeProcess();
}

void terminateShaderCompiler()
{
	glslang::FinalizeProcess();
}

static std::string removeTrailingWhitespace(std::string value)
{
	while (value.size() > 0 && std::isspace(value.back())) value.pop_back();

	return value;
}

Result<CompiledShader> compileShaderSpirV(const std::string& source, const std::string& debugName)
{
	using namespace glslang;

	const char* shaderSource = source.c_str();
	int shaderLength = (int)source.size();

	EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

	const int defaultVersion = 460;

	//This determines what type of shader we are compiling, 
	const EShLanguage language = EShLangCompute;

	std::unique_ptr<TShader> shader = std::make_unique<TShader>(language);
	shader->setStringsWithLengths(&shaderSource, &shaderLength, 1);
	shader->setEnvInput(EShSourceGlsl, language, EShClientVulkan, defaultVersion);
	shader->setEnvClient(EShClientVulkan, EShTargetVulkan_1_0);
	shader->setEnvTarget(EshTargetSpv, EShTargetSpv_1_0);

	if (!shader->parse(&DefaultTBuiltInResource, defaultVersion, false, messages))
	{
		std::stringstream ss;
		ss << "Error generated when compiling shader '" << debugName << "'\n";
		ss << shader->getInfoLog() << "\n";
		ss << shader->getInfoDebugLog();

		return CM_ERROR_MESSAGE(removeTrailingWhitespace(ss.str()));
	}

	std::unique_ptr<TProgram> program = std::make_unique<TProgram>();
	program->addShader(shader.get());

	if (!program->link(messages) || !program->mapIO())
	{
		std::stringstream ss;
		ss << "Error generated when compiling shader '" << debugName << "'\n";
		ss << program->getInfoLog() << "\n";
		ss << program->getInfoDebugLog();

		return CM_ERROR_MESSAGE(removeTrailingWhitespace(ss.str()));
	}

	std::vector<uint32_t> spirv;
	spv::SpvBuildLogger logger;

	SpvOptions spvOptions;
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer = false;
	spvOptions.optimizeSize = false;
	spvOptions.disassemble = false;
	spvOptions.validate = false;
	GlslangToSpv(*program->getIntermediate(language), spirv, &logger, &spvOptions);

	if (logger.getAllMessages().size() > 0)
	{
		std::stringstream ss;
		ss << "Message generated when compiling " << debugName << " shader:" << std::endl;
		ss << logger.getAllMessages();

		return CM_ERROR_MESSAGE(removeTrailingWhitespace(ss.str()));
	}

	return Result<CompiledShader>(spirv);
}