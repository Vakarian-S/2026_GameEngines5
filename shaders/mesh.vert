#version 450

/* One corner's position, read from the vertex buffer. The location matches Vertex.h */
layout (location = 0) in vec3 inPosition;

/* One corner's colour, read from the vertex buffer. The location matches Vertex.h */
layout (location = 1) in vec3 inColor;

/* Goes to the fragment shader. The GPU blends the three corner values across the triangle. */
layout (location = 0) out vec3 fragColor;


/* One object's model matrix, set by vkCmdPushConstants before each draw */
layout (push_constant) uniform PushConstants {
    mat4 modelMatrix;
} pushConstants;

/* The camera’s view and projection matrices, read from the uniform buffer bound at set 0, binding 0 */
layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
} camera;


void main()
{
    gl_Position = camera.projection * camera.view * pushConstants.modelMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
}