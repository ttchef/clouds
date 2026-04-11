
#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

// everything in this file needs to be only preprocessor
// it will be shared among shaders acting as a only source
// of truth

// bindings in global descriptor set
#define GLOBAL_DESC_TEXTURE_BINDING 0
#define GLOBAL_DESC_LIGHT_BINDING 1
#define GLOBAL_DESC_MATRIX_BINDING 2

#define MAX_DIRECTIONAL_LIGHTS 24
#define MAX_POINT_LIGHTS 128
#define MAX_SPOT_LIGHTS 128

#endif // SHADER_SAHRED_H
