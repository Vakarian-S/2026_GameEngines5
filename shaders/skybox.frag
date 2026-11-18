#version 450

/* The world-space direction from skybox.vert, blended across the triangle */
layout (location = 0) in vec3 fragDirection;

/* The sky's six faces, at set 0, binding 3 */
layout (set = 0, binding = 3) uniform samplerCube skyCubeMap;

layout (location = 0) out vec4 outColor;

void main() {
    /* Normalize here, not per corner, or the sky bends near the screen edges */
    vec3 direction = normalize(fragDirection);
    /* Negated to prevent the cubemap from being mirrored, since cubes are left-handed but our world is right-handed */
    vec3 cubeMapDirection = vec3(direction.x, direction.y, -direction.z);
    outColor = vec4(texture(skyCubeMap, cubeMapDirection).rgb, 1.0);
}