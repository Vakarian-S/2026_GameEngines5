/* The fast float parser bundled at commit 62ff207 does not compile under MSVC with C++20 (error C3615). This switches to tinyobjloader's own parser */
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"