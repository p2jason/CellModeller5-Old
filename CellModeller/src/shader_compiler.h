#pragma once

#include "result.h"

#include <cstdint>
#include <vector>
#include <string>

typedef std::vector<unsigned int> CompiledShader;

bool startupShaderCompiler();
void terminateShaderCompiler();

Result<CompiledShader> compileShaderSpirV(const std::string& source, const std::string& debugName = "untitled");