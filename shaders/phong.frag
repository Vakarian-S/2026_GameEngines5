#version 450

/* Must match kMaxLights in Descriptors.h */
#define MAX_LIGHTS 4

/* The camera, read from set 0, binding 0. Only its position is used here */
layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
} camera;

/* One light. The fields match GpuLight */
struct Light {
    vec4 position;
    vec4 color;
};

/* Every light this frame, read from set 0, binding 1. The fields match LightUniform */
layout (set = 0, binding = 1) uniform Lighting {
    Light lights[MAX_LIGHTS];
    vec4 ambient;
    uint count;
} lighting;

/* Material of the current object. The fields match MaterialUniform */
layout (set = 1, binding = 1) uniform Material {
    float shininess;
    float specularStrength;
} material;

/* The texture this draw samples: the set FrameLoop bound as set 1, binding 0 */
layout (set = 1, binding = 0) uniform sampler2D albedo;

/* The locations match the ones phong.vert writes to */
layout (location = 0) in vec3 fragWorldPosition;
layout (location = 1) in vec3 fragNormal;
layout (location = 2) in vec2 fragUV;

/* Location 0 is the first colour attachment, which is the swapchain image. */
layout (location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 toCamera = normalize(camera.position.xyz - fragWorldPosition);
    vec3 surfaceColor = texture(albedo, fragUV).rgb;

/* Ambient is added once, however many lights there are */
    vec3 color = lighting.ambient.rgb * surfaceColor;

    for (uint i = 0; i < lighting.count; ++i)
    {
        vec3 toLight = normalize(lighting.lights[i].position.xyz - fragWorldPosition);
        vec3 lightColor = lighting.lights[i].color.rgb;

    /* Diffuse: brightest where the surface faces the light, zero once it faces away */
        float diffuse = max(dot(normal, toLight), 0.0);
        color += diffuse * lightColor * surfaceColor;

    /* Specular only on the lit side. reflect takes the direction the light travels, so toLight is negated */
        if (diffuse > 0.0)
        {
            vec3 reflected = reflect(-toLight, normal);
            float specular = pow(max(dot(reflected, toCamera), 0.0), material.shininess) * material.specularStrength;
        /* The highlight takes the light's colour, not the texture's */
            color += specular * lightColor;
        }
    }
    outColor = vec4(color, 1.0);
}