#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set=0, binding=0, std430) buffer Positions {
	vec3[] u_positions;
};

layout(set=0, binding=1, std430) buffer Rotations {
	vec2[] u_rotations;
};

layout(set=0, binding=2, std430) buffer Sizes {
	vec2[] u_sizes;
};

layout(set=0, binding=3, std430) buffer Velocities {
	vec3[] u_velocities;
};

layout(push_constant) uniform PushConstants {
	uint c_cellCount;
	float c_deltaTime;
};

void main() {
	uint cellIndex = gl_GlobalInvocationID.x;
	
	if (cellIndex >= c_cellCount) {
		return;
	}
	
	vec3 accel = vec3(0, -9.81, 0);
	u_velocities[cellIndex] += accel * c_deltaTime;
	u_positions[cellIndex] += u_velocities[cellIndex] * c_deltaTime;
}