#version 450

/* The camera, read from the uniform buffer bound at set 0, binding 0. The fields match CameraUniform */
layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
} camera;

layout (location = 0) out vec3 fragDirection;

/* One triangle that covers the whole screen, in NDC. The GPU clips the parts outside it */
const vec2 corners[3] = vec2[](
vec2(-1.0, -1.0),
vec2(3.0, -1.0),
vec2(-1.0, 3.0)
);

void main() {
    vec2 corner = corners[gl_VertexIndex];

    /* z equals w, so the depth after the divide is exactly 1.0, the far end */
    gl_Position = vec4(corner, 1.0, 1.0);

    /* Undo the projection. z = 0.5 is any depth inside the frustum, since only the direction matters */
    vec4 cameraSpacePoint = inverse(camera.projection) * vec4(corner, 0.5, 1.0);
    vec3 cameraSpaceDirection = cameraSpacePoint.xyz / cameraSpacePoint.w;

    /* Undo the view's rotation only, so the camera's position never moves the sky */
    fragDirection = transpose(mat3(camera.view)) * cameraSpaceDirection;
}