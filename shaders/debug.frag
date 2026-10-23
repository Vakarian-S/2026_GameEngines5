#version 450

layout (location = 0) in vec3 fragWorldColor;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragWorldColor, 1.0);
}