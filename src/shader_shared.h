
#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

// everything in this file needs to be only preprocessor
// it will be shared among shaders acting as a only source
// of truth

// bindings in global descriptor set
#define GLOBAL_DESC_TEXTURE_BINDING 0
#define GLOBAL_DESC_LIGHT_BINDING 1
#define GLOBAL_DESC_MATRIX_BINDING 2

#endif // SHADER_SAHRED_H
