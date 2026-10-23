#version 450

layout (location = 0) in vec3 inPosition;

layout (location = 1) in vec3 inColor;

/* The camera, read from the uniform buffer bound at set 0, binding 0. The fields match CameraUniform */
layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
} camera;

layout (location = 0) out vec3 fragWorldColor;

void main() {
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1.0);
    fragWorldColor = inColor;
}