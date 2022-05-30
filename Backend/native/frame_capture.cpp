#include "frame_capture.h"

#include "renderdoc/renderdoc_app.h"

#include <iostream>

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#include <Windows.h>
#define CM5_PLATFORM_WINDOWS
#elif defined(__linux__)
#include <dlfcn.h>
#define CM5_PLATFORM_LINUX
#elif defined(__APPLE__)
#include <dlfcn.h>
#define CM5_PLATFORM_MACOS
#endif

static RENDERDOC_API_1_1_2* g_renderDocApi = nullptr;

void initFrameCapture()
{
	pRENDERDOC_GetAPI RENDERDOC_GetAPI = nullptr;

#if defined(CM5_PLATFORM_WINDOWS)
	if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
	{
		RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
	}
#elif defined(CM5_PLATFORM_LINUX) || defined(CM5_PLATFORM_MACOS)
	if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
	{
		RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
	}
#endif

	if (RENDERDOC_GetAPI && !RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&g_renderDocApi))
	{
		std::cerr << "Failed to acquire RenderDoc API functions" << std::endl;
		g_renderDocApi = nullptr;
	}
}

bool isFrameCaptureSupported()
{
	return g_renderDocApi != nullptr;
}

void beginFrameCapture()
{
	if (g_renderDocApi)
	{
		g_renderDocApi->StartFrameCapture(nullptr, nullptr);
	}
}

void endFrameCapture()
{
	if (g_renderDocApi)
	{
		g_renderDocApi->EndFrameCapture(nullptr, nullptr);
	}
}