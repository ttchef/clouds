
#ifndef RENDERER_H
#define RENDERER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.h"

struct api_version {
    u32 major;
    u32 minor;
    u32 patch;
};

struct rcontext {
    VkInstance instance;
};

bool renderer_init(struct rcontext *rctx);
void renderer_deint(struct rcontext *rctx);

#endif // RENDERER_H
