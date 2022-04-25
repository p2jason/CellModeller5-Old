#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) readonly buffer Positions {
	vec3[] u_positions;
};

layout(set = 0, binding = 1) readonly buffer Rotations {
	vec2[] u_rotations;
};

layout(set = 0, binding = 2) readonly buffer Sizes {
	vec2[] u_sizes;
};

layout(push_constant) uniform PushConstants {
	uint c_cellCount;
};

void main() {
	uint cellIndex = gl_GlobalInvocationID.x;
	
	if (cellIndex >= c_cellCount) {
		return;
	}
	
	
}