#version 450

/* One corner's position, read from the vertex buffer. The location matches Vertex.h */
layout (location = 0) in vec3 inPosition;

/* One corner's colour, read from the vertex buffer. The location matches Vertex.h */
layout (location = 1) in vec3 inColor;

/* Goes to the fragment shader. The GPU blends the three corner values across the triangle. */
layout (location = 0) out vec3 fragColor;

/* TEMPORARY: fixed turn angles, so three faces of the cube show. The camera replaces them */
const float angleY = radians(35.0);
const float angleX = radians(25.0);

void main()
{
    vec3 position = inPosition;

/* Turn around the Y axis: x and z change, y stays */
    position = vec3(
    position.x * cos(angleY) + position.z * sin(angleY),
    position.y,
    -position.x * sin(angleY) + position.z * cos(angleY));

/* Turn around the X axis: y and z change, x stays */
    position = vec3(
    position.x,
    position.y * cos(angleX) - position.z * sin(angleX),
    position.y * sin(angleX) + position.z * cos(angleX));

/* Vulkan keeps only depth from 0 to 1, so move z from the -1 to 1 range into it */
    gl_Position = vec4(position.x, position.y, position.z * 0.5 + 0.5, 1.0);
    fragColor = inColor;
}