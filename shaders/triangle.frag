#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 viewPos;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 uObjectColor;

void main()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-lightDirection);
    vec3 viewDirection = normalize(viewPos - FragPos);

    float diffuseStrength = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * lightColor;

    vec3 reflectedLight = reflect(-lightDir, normal);
    float specularStrength = pow(max(dot(viewDirection, reflectedLight), 0.0), 32.0);
    vec3 specular = 0.25 * specularStrength * lightColor;

    vec3 ambient = 0.2 * lightColor;
    vec3 lighting = ambient + diffuse + specular;
    FragColor = vec4(lighting * uObjectColor, 1.0);
}
