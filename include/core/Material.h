#pragma once

#include "Handles.h"

/** What a material is made of. The store copies it into GPU memory once, when the material is created. */
struct MaterialDesc
{
    /** The texture the surface shows. */
    TextureHandle albedo;
    /** How tight the highlight is. Higher values make a smaller, sharper highlight. */
    float shininess;
    /** How bright the highlight is, from 0 for none to 1 for the light's full colour. */
    float specularStrength;
};
