#version 450

layout(set = 0, binding = 0) readonly buffer Positions {
	vec3[] positions;
};

layout(set = 0, binding = 1) readonly buffer Rotations {
	vec2[] rotations;
};

layout(set = 0, binding = 2) readonly buffer Sizes {
	vec2[] sizes;
};

void main() {
	
}