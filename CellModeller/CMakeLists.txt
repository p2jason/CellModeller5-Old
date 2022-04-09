cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(CellModeller5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

############ Setup Vulkan-Headers ############
set(VK_HEADERS_INCLUDE "${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include")

add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE ${VK_HEADERS_INCLUDE})

############ Setup volk ############
set(VOLK_ROOT "${PROJECT_SOURCE_DIR}/third_party/volk")

add_library(volk ${VOLK_ROOT}/volk.h ${VOLK_ROOT}/volk.c)

target_include_directories(volk PUBLIC ${VOLK_ROOT}/)
target_link_libraries(volk Vulkan-Headers)

# set_target_properties(volk PROPERTIES LINKER_LANGUAGE CXX)

########### Setup glslang ############
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
set(ENABLE_CTEST OFF CACHE BOOL "")
set(ENABLE_HLSL OFF CACHE BOOL "")

add_subdirectory(third_party/glslang)

############ Setup pynind11 ############
set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
set(PYBIND11_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/pybind11)

############ Setup sources ############
if (WIN32)
   set(GLOBAL_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif()
   # Other platforms
endif()

file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.h")

pybind11_add_module(${PROJECT_NAME} ${SOURCE_FILES})

include_directories("${PROJECT_SOURCE_DIR}/src/")

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan-Headers)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang)
target_link_libraries(${PROJECT_NAME} PRIVATE SPIRV)

target_include_directories(${PROJECT_NAME} PUBLIC src)

target_compile_definitions(${PROJECT_NAME} PRIVATE ${GLOBAL_DEFINES})

############ Setup IDE filters ############
macro(ConvertToFilters curdir)
	file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			ConvertToFilters(${curdir}/${child})
		else()
			string(REPLACE "/" "\\" groupname ${curdir})
			
			source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
		endif()
	endforeach()
endmacro()

ConvertToFilters(data)
ConvertToFilters(src)