#version 450

/* The blended colour from the vertex shader. The location must match the one it wrote to. */
layout (location = 0) in vec3 fragColor;

/* Location 0 is the first colour attachment, which is the swapchain image. */
layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}