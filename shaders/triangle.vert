#version 450

/*
 * The triangle's three corners, in clip space. X runs from -1 at the left edge to 1 at the
 * right edge. Y runs from -1 at the top edge to 1 at the bottom edge, so Y points down.
 */
vec2 positions[3] = vec2[](
vec2( 0.0, -0.5),
vec2( 0.5,  0.5),
vec2(-0.5,  0.5)
);

/* One colour per corner: red, green, blue. */
vec3 colors[3] = vec3[](
vec3(1.0, 0.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0)
);

/* Goes to the fragment shader. The GPU blends the three corner values across the triangle. */
layout (location = 0) out vec3 fragColor;

void main()
{
/* gl_VertexIndex counts 0, 1, 2 because the draw call asks for three vertices. */
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}