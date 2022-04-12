#version 300 es

#pragma vscode_glsllint_stage : vert
#define MATH_PI (3.14159265359)

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;

layout(location = 3) in vec3 a_CellPos;
layout(location = 4) in vec3 a_CellDir;
layout(location = 5) in float a_Length;
layout(location = 6) in float a_Radius;
layout(location = 7) in vec4 a_Color;

uniform mat4 u_MvpMatrix;

out vec3 v_WorldPos;
out float v_Radius;
out vec3 v_CellEnd0;
out vec3 v_CellEnd1;

out vec3 v_Color;

void main() {
	//Calculate rotation matrix
	float yaw = atan(a_CellDir.x, a_CellDir.z);
	float pitch = acos(a_CellDir.y);

	float cy = cos(yaw);
	float sy = sin(yaw);
	float cp = cos(pitch);
	float sp = sin(pitch);

	mat3 rotMatrix = mat3(
		cy, 0.0, -sy,
		sy * sp, cp, cy * sp, 
		sy * cp, -sp, cy * cp
	);

	//Transform vertex position to world space
	mat3 scaleMatrix = mat3(1.0);
	scaleMatrix[0][0] = a_Radius;
	scaleMatrix[1][1] = 0.5;
	scaleMatrix[2][2] = a_Radius;

	mat4 modelMatrix = mat4(rotMatrix * scaleMatrix);
	modelMatrix[3] = vec4(a_CellPos, 1.0);

	vec3 position = a_Position;
	position.y += mix(-1.0, 1.0, a_TexCoords.x) * (a_Length - 1.0);

	vec4 worldPos = modelMatrix * vec4(position, 1.0);
	
	//Write varyings
	gl_Position = u_MvpMatrix * worldPos;

	v_CellEnd0 = vec3(modelMatrix * vec4(0.0,  a_Length - 1.0, 0.0, 1.0));
	v_CellEnd1 = vec3(modelMatrix * vec4(0.0, -a_Length + 1.0, 0.0, 1.0));

	v_WorldPos = worldPos.xyz;
//	v_Normal = rotMatrix * a_Normal;
	v_Color = a_Color.xyz;
}