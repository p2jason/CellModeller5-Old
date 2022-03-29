#version 300 es

#pragma vscode_glsllint_stage : frag
precision highp float;

uniform vec3 u_CameraPos;

in vec3 v_WorldPos;
in float v_Radius;
in vec3 v_CellEnd0;
in vec3 v_CellEnd1;

in vec3 v_Color;

out vec4 outColor;

/*
 Based on:
	https://stackoverflow.com/questions/2824478/shortest-distance-between-two-line-segments
*/
float lineSegmentDistance(vec3 a0, vec3 a1, vec3 b0, vec3 b1) {
	vec3 A = a1 - a0;
	vec3 B = b1 - b0;
	
	float magA = length(A);
	float magB = length(B);
	
	vec3 _A = A / magA;
	vec3 _B = B / magB;
	
	vec3 crs = cross(_A, _B);
	float denom = dot(crs, crs);
	
	vec3 t = b0 - a0;
	
	float detA = determinant(mat3(t, _B, crs));
	float detB = determinant(mat3(t, _A, crs));
	
	float t0 = detA / denom;
	float t1 = detB / denom;
	
	vec3 pA = a0 + (_A * t0);
	vec3 pB = b0 + (_B * t1);
	
	if (t1 < 0.0) pB = b0;
	else if (t1 > magB) pB = b1;

	if ((t1 < 0.0) || (t1 > magB)) {
		pA = a0 + _A * dot(_A, (pB - a0));
	}
	
	return distance(pA, pB);
}

void main() {
	float dist = lineSegmentDistance(u_CameraPos, v_WorldPos, v_CellEnd0, v_CellEnd1);

	vec3 color = mix(v_Color, vec3(1.0), smoothstep(0.39, 0.42, dist));

	if (dist > 0.5) {
		discard;
	}

	outColor = vec4(color, 1);
}