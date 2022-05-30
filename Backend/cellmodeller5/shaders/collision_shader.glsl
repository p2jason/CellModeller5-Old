#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

/******* Input state *******/
layout(set=0, binding=0, std430) buffer InputPositions {
	vec3[] u_inputPositions;
};

layout(set=0, binding=1, std430) buffer InputRotations {
	vec2[] u_inputRotations;
};

layout(set=0, binding=2, std430) buffer InputSizes {
	vec2[] u_inputSizes;
};

layout(set=0, binding=3, std430) buffer InputVelocities {
	vec3[] u_inputVelocities;
};

/******* Output state *******/
layout(set=1, binding=0, std430) buffer OutputPositions {
	vec3[] u_outputPositions;
};

layout(set=1, binding=1, std430) buffer OutputRotations {
	vec2[] u_outputRotations;
};

layout(set=1, binding=2, std430) buffer OutputSizes {
	vec2[] u_outputSizes;
};

layout(set=1, binding=3, std430) buffer OutputVelocities {
	vec3[] u_outputVelocities;
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

	vec3 totalForce = vec3(0);
	vec3 currentPosition = u_inputPositions[cellIndex];

	vec3 accel = totalForce;
	vec3 velocity = u_inputVelocities[cellIndex] + accel * c_deltaTime;

	u_outputVelocities[cellIndex] = velocity;
	u_outputPositions[cellIndex] = currentPosition + velocity * c_deltaTime;
}