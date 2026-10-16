#version 450

/* The texture this draw samples: the set FrameLoop bound as set 1, binding 0 */
layout (set = 1, binding = 0) uniform sampler2D albedo;

/* The blended colour from the vertex shader. The location must match the one it wrote to. */
layout (location = 0) in vec3 fragColor;

layout (location = 1) in vec2 fragUV;

/* Location 0 is the first colour attachment, which is the swapchain image. */
layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(albedo, fragUV);
}