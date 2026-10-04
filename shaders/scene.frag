#version 460 core

in vec3 fragmentPosition;
in vec3 fragmentNormal;
in vec2 fragmentTexCoords;

uniform vec3 viewPos;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float ambientStrength;
uniform sampler2D uDiffuseMap;
uniform bool uUseTexture;
uniform vec3 uObjectColor;
uniform bool uSelectedLink;

out vec4 color;

void main()
{
	vec3 baseColor = uUseTexture
		? texture(uDiffuseMap, fragmentTexCoords).rgb
		: uObjectColor;
	vec3 normal = normalize(fragmentNormal);
	vec3 light = normalize(-lightDirection);
	float diffuseStrength = max(dot(normal, light), 0.0);
	vec3 viewDirection = normalize(viewPos - fragmentPosition);
	vec3 reflectedLight = reflect(-light, normal);
	float specularStrength = pow(max(dot(viewDirection, reflectedLight), 0.0), 32.0);
	vec3 ambient = ambientStrength * ambientColor * baseColor;
	vec3 lighting = ambient + diffuseStrength * baseColor * lightColor;
	lighting += 0.25 * specularStrength * lightColor;
	if (uSelectedLink)
		lighting = mix(lighting, vec3(0.98, 0.66, 0.16), 0.72);
	color = vec4(lighting, 1.0);
}
