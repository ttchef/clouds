
#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

// everything in this file needs to be only preprocessor
// it will be shared among shaders acting as a only source
// of truth

// bindings in global descriptor set
#define GLOBAL_DESC_TEXTURE_BINDING 0
#define GLOBAL_DESC_LIGHT_BINDING 1
#define GLOBAL_DESC_MATRIX_BINDING 2
#define GLOBAL_DESC_SHADOW_DIRECTIONAL_BINDING 3
#define GLOBAL_DESC_SHADOW_POINT_BINDING 4
#define GLOBAL_DESC_SHADOW_SPOT_BINDING 5
#define GLOBAL_DESC_SKYBOX_BINDING 6
#define GLOBAL_DESC_NOISE_3D_BINDING 7

#define MAX_DIRECTIONAL_LIGHTS 24
#define MAX_POINT_LIGHTS 128
#define MAX_SPOT_LIGHTS 128

// total max of different textures to exist
#define MAX_TEXTURES 1024

#endif // SHADER_SAHRED_H
