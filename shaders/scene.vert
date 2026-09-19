#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragmentPosition;
out vec3 fragmentNormal;
out vec2 fragmentTexCoords;

void main()
{
	vec4 worldPosition = model * vec4(aPosition, 1.0);
	fragmentPosition = worldPosition.xyz;
	fragmentNormal = mat3(transpose(inverse(model))) * aNormal;
	fragmentTexCoords = aTexCoords;
	gl_Position = projection * view * worldPosition;
}
