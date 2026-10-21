    #version 450

/* One corner's position, read from the vertex buffer. The location matches Vertex.h */
layout (location = 0) in vec3 inPosition;

/* One corner's normal, read from the vertex buffer. The location matches Vertex.h */
layout (location = 1) in vec3 inNormal;

/* Where the corner sits on the texture. The location matches Vertex.h */
layout (location = 2) in vec2 inUV;

/* The corner's place in the world. The fragment shader measures the light and camera directions from it */
layout (location = 0) out vec3 fragWorldPosition;

/* The corner's normal in world space. Blending across the triangle shortens it, so the fragment shader normalizes it again */
layout (location = 1) out vec3 fragNormal;

layout (location = 2) out vec2 fragUV;

/* One object's matrices, set by vkCmdPushConstants before each draw. The fields match ModelMatrixPushConstant */
layout (push_constant) uniform PushConstants {
    mat4 modelMatrix;
    /* The inverse transpose of the model matrix, built once per object on the CPU */
    mat3 normalMatrix;
} pushConstants;

/* The camera, read from the uniform buffer bound at set 0, binding 0. The fields match CameraUniform */
layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
} camera;

void main()
{
    vec4 worldPosition = pushConstants.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = camera.projection * camera.view * worldPosition;

    fragWorldPosition = worldPosition.xyz;
    fragNormal = pushConstants.normalMatrix * inNormal;
    fragUV = inUV;
}