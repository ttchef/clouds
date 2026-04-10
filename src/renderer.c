
#include "renderer.h"
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <cgltf/cgltf.h>
#include <stbi/stb_image.h>

#include "cmath.h"
#include "darray.h"
#include "log.h"

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

// push constant
struct model_color_pc {
    matrix mat;
    vec4 color;
};

struct model_texture_pc {
    matrix mat;
    u32 texture_index;
};

static struct api_version get_api_version() {
    u32 instance_version;
    if (vkEnumerateInstanceVersion(&instance_version) != VK_SUCCESS) {
        LOGM(ERROR, "failed to get instance version");
        exit(1);
    }

    return (struct api_version){
        .major = VK_API_VERSION_MAJOR(instance_version),
        .minor = VK_API_VERSION_MINOR(instance_version),
        .patch = VK_API_VERSION_PATCH(instance_version),
    };
}

static bool create_instance(struct rcontext *c, i32 n_exts, const char **exts,
                            i32 n_layers, const char **layers) {
    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_4,
        .applicationVersion = VK_MAKE_VERSION(6, 7, 0),
        .engineVersion = VK_MAKE_VERSION(6, 7, 0),
        .pApplicationName = "fire app 67",
        .pEngineName = "fire engine 67",
    };

    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = n_exts,
        .ppEnabledExtensionNames = exts,
        .enabledLayerCount = n_layers,
        .ppEnabledLayerNames = layers,
    };

    if (vkCreateInstance(&create_info, NULL, &c->instance) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create instance");
        return false;
    }

    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    (void)message_severity;
    (void)message_type;
    (void)user_data;

    LOGM(WARN, "Validation: %s", callback_data->pMessage);

    return VK_FALSE;
}

static bool create_db_messenger(struct rcontext *c) {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            c->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!vkCreateDebugUtilsMessengerEXT) {
        LOGM(ERROR, "failed to load debug utils messenger function pointer");
        return false;
    }

    if (vkCreateDebugUtilsMessengerEXT(c->instance, &create_info, NULL,
                                       &c->db_messenger) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create debug messenger");
        return false;
    }

    return true;
}

static bool create_surface(struct rcontext *c, GLFWwindow *window) {
    glfwCreateWindowSurface(c->instance, window, NULL, &c->surface);
    if (!c->surface) {
        LOGM(ERROR, "failed to create surface");
        return false;
    }

    return true;
}

static bool create_phy_dev(struct rcontext *c) {
    u32 n_phys_dev;
    vkEnumeratePhysicalDevices(c->instance, &n_phys_dev, NULL);
    if (n_phys_dev == 0) {
        LOGM(ERROR, "failed to find GPU which supports vulkan");
        return false;
    }

    // no one has more than 8 gpus :sob:
    VkPhysicalDevice devs[8];
    vkEnumeratePhysicalDevices(c->instance, &n_phys_dev, devs);

    for (u32 i = 0; i < n_phys_dev; i++) {
        VkPhysicalDevice dev = devs[i];
        u32 n_queues;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &n_queues, NULL);

        VkQueueFamilyProperties props[8];
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &n_queues, props);

        i32 graphics_queue_family_index = -1;
        i32 present_queue_family_index = -1;

        for (u32 j = 0; j < n_queues; j++) {
            if (props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_queue_family_index = j;
            }

            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, j, c->surface,
                                                 &supported);
            if (supported) {
                present_queue_family_index = j;
                if (graphics_queue_family_index != -1)
                    break;
            }
        }

        if (graphics_queue_family_index != -1 &&
            present_queue_family_index != -1) {

            c->phy_dev = devs[i];
            c->present_queue.index = present_queue_family_index;
            c->graphics_queue.index = graphics_queue_family_index;

            break;
        } else if (i == n_phys_dev - 1) {
            LOGM(ERROR, "failed to find sutable GPU");
            return false;
        }
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(c->phy_dev, &props);
    LOGM(INFO, "picked GPU: %s", props.deviceName);

    return true;
}

static bool create_log_dev(struct rcontext *c) {
    VkDeviceQueueCreateInfo queue_infos[2];

    float priority = 1.0f;

    u32 n_queues = 0;
    queue_infos[n_queues++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = c->graphics_queue.index,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    if (c->graphics_queue.index != c->present_queue.index) {
        queue_infos[n_queues++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = c->present_queue.index,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
    }

    const char *device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceDescriptorIndexingFeatures indexing = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .runtimeDescriptorArray = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dym_rendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = true,
        .pNext = &indexing,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dym_rendering,
        .pQueueCreateInfos = queue_infos,
        .queueCreateInfoCount = n_queues,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_exts,
    };

    if (vkCreateDevice(c->phy_dev, &create_info, NULL, &c->dev) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create logical device");
        return false;
    }

    vkGetDeviceQueue(c->dev, c->graphics_queue.index, 0,
                     &c->graphics_queue.handle);
    vkGetDeviceQueue(c->dev, c->present_queue.index, 0,
                     &c->present_queue.handle);

    return true;
}

static bool create_image(struct rcontext *c, struct image *image, u32 width,
                         u32 height, VkFormat fmt, VkImageUsageFlags usage) {
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = (VkExtent3D){width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = fmt,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    if (vmaCreateImage(c->allocator, &image_create_info, &alloc_info,
                       &image->handle, &image->alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create vulkan image");
        return false;
    }

    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = fmt,
        .subresourceRange =
            {
                .aspectMask = fmt == VK_FORMAT_D32_SFLOAT
                                  ? VK_IMAGE_ASPECT_DEPTH_BIT
                                  : VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
    };

    if (vkCreateImageView(c->dev, &view_create_info, NULL, &image->view) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create vulkan image");
        return false;
    }

    return true;
}

static void destroy_image(struct rcontext *c, struct image *image) {
    vkDestroyImageView(c->dev, image->view, NULL);
    vmaDestroyImage(c->allocator, image->handle, image->alloc);
}

static bool create_swapchain(struct rcontext *c, u32 w, u32 h) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c->phy_dev, c->surface, &caps);

    u32 n_fmts;
    vkGetPhysicalDeviceSurfaceFormatsKHR(c->phy_dev, c->surface, &n_fmts, NULL);
    VkSurfaceFormatKHR fmts[n_fmts];
    vkGetPhysicalDeviceSurfaceFormatsKHR(c->phy_dev, c->surface, &n_fmts, fmts);

    VkSurfaceFormatKHR fmt = fmts[0];
    for (u32 i = 0; i < n_fmts; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8_SRGB &&
            fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = fmts[i];
            break;
        }
    }

    u32 n_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(c->phy_dev, c->surface,
                                              &n_present_modes, NULL);
    VkPresentModeKHR present_modes[n_present_modes];
    vkGetPhysicalDeviceSurfacePresentModesKHR(c->phy_dev, c->surface,
                                              &n_present_modes, present_modes);

    VkPresentModeKHR present_mode = present_modes[0];
    for (u32 i = 0; i < n_present_modes; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = present_modes[i];
            break;
        }
    }

    VkExtent2D extent = (VkExtent2D){
        .width = w,
        .height = h,
    };

    extent.width = MIN(caps.maxImageExtent.width, extent.width);
    extent.height = MIN(caps.maxImageExtent.height, extent.height);
    extent.width = MAX(caps.minImageExtent.width, extent.width);
    extent.height = MAX(caps.minImageExtent.height, extent.height);

    u32 n_imgs = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && n_imgs > caps.maxImageCount) {
        n_imgs = caps.maxImageCount;
    }
    LOGM(API_DUMP, "swapchain image count: %d", n_imgs);

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = c->surface,
        .clipped = VK_TRUE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageFormat = fmt.format,
        .imageColorSpace = fmt.colorSpace,
        .presentMode = present_mode,
        .imageExtent = extent,
        .minImageCount = n_imgs,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    };

    if (vkCreateSwapchainKHR(c->dev, &create_info, NULL,
                             &c->swapchain.handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create swapchain");
        return false;
    }

    LOGM(API_DUMP, "Swapchain Size: %d | %d", extent.width, extent.height);

    c->swapchain.fmt = fmt.format;
    c->swapchain.extent = extent;

    vkGetSwapchainImagesKHR(c->dev, c->swapchain.handle, &c->swapchain.n_imgs,
                            NULL);
    c->swapchain.imgs = calloc(c->swapchain.n_imgs, sizeof(VkImage));
    vkGetSwapchainImagesKHR(c->dev, c->swapchain.handle, &c->swapchain.n_imgs,
                            c->swapchain.imgs);
    c->swapchain.imgs_views = calloc(c->swapchain.n_imgs, sizeof(VkImageView));
    c->swapchain.depth_images =
        calloc(c->swapchain.n_imgs, sizeof(struct image));

    for (u32 i = 0; i < c->swapchain.n_imgs; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = c->swapchain.imgs[i],
            .format = c->swapchain.fmt,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                    .levelCount = 1,
                },
        };

        if (vkCreateImageView(c->dev, &create_info, NULL,
                              &c->swapchain.imgs_views[i]) != VK_SUCCESS) {
            LOGM(ERROR, "Failed to create image view: %d", i);

            free(c->swapchain.imgs_views);
            c->swapchain.imgs_views = NULL;

            free(c->swapchain.imgs);
            c->swapchain.imgs = NULL;

            return false;
        }

        create_image(c, &c->swapchain.depth_images[i], w, h,
                     VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    return true;
}

static void destroy_swapchain(struct rcontext *c) {
    if (c->swapchain.imgs) {
        free(c->swapchain.imgs);
    }

    if (c->swapchain.imgs_views) {
        for (u32 i = 0; i < c->swapchain.n_imgs; i++) {
            vkDestroyImageView(c->dev, c->swapchain.imgs_views[i], NULL);
        }
        free(c->swapchain.imgs_views);
    }

    if (c->swapchain.depth_images) {
        for (u32 i = 0; i < c->swapchain.n_imgs; i++) {
            destroy_image(c, &c->swapchain.depth_images[i]);
        }
        free(c->swapchain.depth_images);
    }

    vkDestroySwapchainKHR(c->dev, c->swapchain.handle, NULL);
}

static bool create_shader_module(struct rcontext *c, VkShaderModule *module,
                                 const char *filename) {
    FILE *shader_fd = fopen(filename, "rb");
    if (!shader_fd) {
        LOGM(ERROR, "failed to open shader file: %s", filename);
        return false;
    }

    fseek(shader_fd, 0, SEEK_END);
    i64 shader_size = ftell(shader_fd);
    rewind(shader_fd);

    if ((shader_size & 0x03) != 0) {
        LOGM(ERROR, "shader compile is not a multiple of 4");
        fclose(shader_fd);
        return false;
    }

    // doesnt need to be null terminated because vulkan takes it len based
    // atleast thats what i think
    u8 shader_str[shader_size];
    fread(shader_str, 1, shader_size, shader_fd);
    fclose(shader_fd);

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_size,
        .pCode = (u32 *)shader_str,
    };

    if (vkCreateShaderModule(c->dev, &create_info, NULL, module) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create shader module: %s", filename);
        return false;
    }

    return true;
}

static bool create_pipeline(struct rcontext *c, struct pipeline *pipeline,
                            const char *vertex_path, const char *fragment_path,
                            VkVertexInputBindingDescription binding_decs,
                            VkVertexInputAttributeDescription *attribute_decs,
                            u32 n_attributes,
                            VkPipelineLayoutCreateInfo layout_info) {
    VkShaderModule vert_module;
    if (!create_shader_module(c, &vert_module, vertex_path)) {
        return false;
    }

    LOGM(API_DUMP, "created vertex shader module");

    VkShaderModule frag_module;
    if (!create_shader_module(c, &frag_module, fragment_path)) {
        return false;
    }

    LOGM(API_DUMP, "created fragment shader module");

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        },
    };

    VkPipelineRenderingCreateInfo dynamic_rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &c->swapchain.fmt,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_decs,
        .vertexAttributeDescriptionCount = n_attributes,
        .pVertexAttributeDescriptions = attribute_decs,
    };

    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
        .depthClampEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkDynamicState dym_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dym_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_COUNT(dym_states),
        .pDynamicStates = dym_states,
    };

    if (vkCreatePipelineLayout(c->dev, &layout_info, NULL, &pipeline->layout) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create pipeline layout");
        goto error_path;
    }

    LOGM(API_DUMP, "created pipeline layout");

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &dynamic_rendering,
        .layout = pipeline->layout,
        .stageCount = ARRAY_COUNT(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex,
        .pInputAssemblyState = &assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend,
        .pDynamicState = &dym_state,
        .renderPass = VK_NULL_HANDLE, // better safe than sorry xD
    };

    if (vkCreateGraphicsPipelines(c->dev, 0, 1, &create_info, NULL,
                                  &pipeline->handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create graphics pipeline");
        goto error_path;
    }

    vkDestroyShaderModule(c->dev, vert_module, NULL);
    vkDestroyShaderModule(c->dev, frag_module, NULL);

    return true;

error_path:
    vkDestroyShaderModule(c->dev, vert_module, NULL);
    vkDestroyShaderModule(c->dev, frag_module, NULL);
    return false;
}

static void destroy_pipeline(struct rcontext *c, struct pipeline *pipeline) {
    vkDestroyPipelineLayout(c->dev, pipeline->layout, NULL);
    vkDestroyPipeline(c->dev, pipeline->handle, NULL);
}

static bool create_frame_data(struct rcontext *c) {
    c->frame_idx = 0;
    c->img_idx = 0;

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = c->graphics_queue.index,
    };

    if (vkCreateCommandPool(c->dev, &pool_create_info, NULL, &c->cmd_pool) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command pool");
        return false;
    }

    LOGM(API_DUMP, "created command pool");

    VkSemaphoreCreateInfo sem_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = c->cmd_pool,
            .commandBufferCount = 1,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };

        if (vkAllocateCommandBuffers(c->dev, &alloc_info,
                                     &c->frame_data[i].cmd_buffer) !=
            VK_SUCCESS) {
            LOGM(ERROR, "failed to allocate command buffer: %d", i);
            return false;
        }

        LOGM(API_DUMP, "created command buffer: %d", i);

        if (vkCreateSemaphore(c->dev, &sem_create_info, NULL,
                              &c->frame_data[i].image_available) !=
            VK_SUCCESS) {
            LOGM(ERROR, "failed to create image availabe semaphore: %d", i);
            return false;
        }

        LOGM(API_DUMP, "created image available semaphor: %d", i);

        if (vkCreateSemaphore(c->dev, &sem_create_info, NULL,
                              &c->finished[i]) != VK_SUCCESS) {
            LOGM(ERROR, "failed to create image availabe semaphore: %d", i);
            return false;
        }

        LOGM(API_DUMP, "created finished semaphor: %d", i);

        if (vkCreateFence(c->dev, &fence_create_info, NULL,
                          &c->frame_data[i].in_flight_fence) != VK_SUCCESS) {
            LOGM(ERROR, "failed to create fence: %d", i);
            return false;
        }

        LOGM(API_DUMP, "created fence: %d", i);
    }

    return true;
}

static bool create_vma(struct rcontext *c) {
    VmaAllocatorCreateInfo create_info = {
        .physicalDevice = c->phy_dev,
        .device = c->dev,
        .instance = c->instance,
    };

    if (vmaCreateAllocator(&create_info, &c->allocator) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create vma");
        return false;
    }

    return true;
}

static struct buffer create_device_local_buffer(struct rcontext *c,
                                                VkDeviceSize size,
                                                VkBufferUsageFlags flags) {
    struct buffer res = {0};

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    if (vmaCreateBuffer(c->allocator, &create_info, &alloc_info, &res.handle,
                        &res.alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create device local buffer");
        return res;
    }

    res.size = size;

    return res;
}

static struct buffer
create_staging_buffer(struct rcontext *c, VkDeviceSize size, const void *data) {
    struct buffer res = {0};

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage =
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = size,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };

    if (vmaCreateBuffer(c->allocator, &create_info, &alloc_info, &res.handle,
                        &res.alloc, NULL) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create device local buffer");
        return res;
    }

    res.size = size;

    void *mapped;
    vmaMapMemory(c->allocator, res.alloc, &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(c->allocator, res.alloc);

    return res;
}

static bool copy_buffer(struct rcontext *c, struct buffer *staging,
                        struct buffer *buffer) {
    // dont handle at the moment
    if (buffer->size != staging->size) {
        LOGM(ERROR, "buffer size and staing buffer size are not equal");
        return false;
    }

    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = c->graphics_queue.index,
    };

    if (vkCreateCommandPool(c->dev, &pool_create_info, NULL, &cmd_pool) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create one time use command pool for staging "
                    "buffer copy");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    if (vkAllocateCommandBuffers(c->dev, &alloc_info, &cmd_buffer) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create commnad buffer for staging buffer copy");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(
            ERROR,
            "failed to begin recording command buffer for staging buffer copy");
        vkDestroyCommandPool(c->dev, cmd_pool, NULL);
        return false;
    }

    VkBufferCopy region = {
        .size = buffer->size,
    };

    vkCmdCopyBuffer(cmd_buffer, staging->handle, buffer->handle, 1, &region);

    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR,
             "failed to end recording command buffer for staging buffer copy");
        vkDestroyCommandPool(c->dev, cmd_pool, NULL);
        return false;
    }

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };

    if (vkQueueSubmit(c->graphics_queue.handle, 1, &sub_info, VK_NULL_HANDLE) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to submit command buffer for staging buffer copy");
        vkDestroyCommandPool(c->dev, cmd_pool, NULL);
        return false;
    }

    if (vkQueueWaitIdle(c->graphics_queue.handle) != VK_SUCCESS) {
        LOGM(ERROR, "failed to wait for queues");
        vkDestroyCommandPool(c->dev, cmd_pool, NULL);
        return false;
    }

    vkDestroyCommandPool(c->dev, cmd_pool, NULL);

    return true;
}

static bool upload_data_to_buffer(struct rcontext *c, struct buffer *buffer,
                                  u32 data_size, void *data) {
    if (data_size != buffer->size) {
        LOGM(WARN, "buffer->size != data_size");
        return false;
    }

    struct buffer staging = create_staging_buffer(c, data_size, data);
    copy_buffer(c, &staging, buffer);
    vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);

    return true;
}

static bool upload_data_to_image(struct rcontext *c, struct image *image,
                                 u32 data_size, void *data, u32 width,
                                 u32 height, VkImageLayout final_layout,
                                 VkAccessFlags dst_access_mask) {
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = c->graphics_queue.index,
    };

    if (vkCreateCommandPool(c->dev, &pool_create_info, NULL, &cmd_pool) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command pool");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
        .commandPool = cmd_pool,
    };

    if (vkAllocateCommandBuffers(c->dev, &alloc_info, &cmd_buffer) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create command buffer");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(ERROR, "failed to begin recording into command buffer");
        return false;
    }

    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1,
                         &image_barrier);

    struct buffer staging = create_staging_buffer(c, data_size, data);

    VkBufferImageCopy region = {
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.layerCount = 1,
        .imageExtent = (VkExtent3D){width, height, 1},
    };

    vkCmdCopyBufferToImage(cmd_buffer, staging.handle, image->handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    image_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = final_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = dst_access_mask,
    };

    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0,
                         1, &image_barrier);
    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR, "failed recording into command buffer");
        vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);
        return false;
    }

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };

    if (vkQueueSubmit(c->graphics_queue.handle, 1, &sub_info, VK_NULL_HANDLE) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to submit queue");
        vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);
        return false;
    }

    if (vkQueueWaitIdle(c->graphics_queue.handle) != VK_SUCCESS) {
        LOGM(ERROR, "Failed to wait for queue");
        vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);
        return false;
    }

    vkDestroyCommandPool(c->dev, cmd_pool, NULL);

    vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);

    return true;
}

static bool create_model_color_pipeline(struct rcontext *c) {
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(struct model_color_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (!create_pipeline(
            c, &c->model_color_pip, "build/spv/model_color-vert.spv",
            "build/spv/model_color-frag.spv", binding_desc, attrib_desc,
            ARRAY_COUNT(attrib_desc), layout_create_info)) {
        return false;
    }

    return true;
}

static bool create_model_texture_pipeline(struct rcontext *c) {
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 8,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 3,
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 5,
        },
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(struct model_texture_pc),
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
        .setLayoutCount = 1,
        .pSetLayouts = &c->texture_manager.layout,
    };

    if (!create_pipeline(
            c, &c->model_texture_pip, "build/spv/model_texture-vert.spv",
            "build/spv/model_texture-frag.spv", binding_desc, attrib_desc,
            ARRAY_COUNT(attrib_desc), layout_create_info)) {
        return false;
    }

    return true;
}

static bool create_sampler(struct rcontext *c) {
    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = create_info.addressModeU,
        .addressModeW = create_info.addressModeU,
        .mipLodBias = 0.0f,
        .maxAnisotropy = 1.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f,
    };

    if (vkCreateSampler(c->dev, &create_info, NULL, &c->sampler) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create sampler");
        return false;
    }

    return true;
}

static bool create_texture_manager(struct rcontext *c) {
    VkDescriptorPoolSize pool_sizes[] = {
        {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            MAX_TEXTURES * FRAMES_IN_FLIGHT,
        },
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAMES_IN_FLIGHT,
        .poolSizeCount = ARRAY_COUNT(pool_sizes),
        .pPoolSizes = pool_sizes,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    };

    if (vkCreateDescriptorPool(c->dev, &pool_create_info, NULL,
                               &c->texture_manager.pool) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create descriptor pool");
        return false;
    }

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES,
         VK_SHADER_STAGE_FRAGMENT_BIT, 0},
    };

    VkDescriptorBindingFlags binding_flags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = ARRAY_COUNT(bindings),
        .pBindingFlags = &binding_flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = bindings,
        .bindingCount = ARRAY_COUNT(bindings),
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .pNext = &flags_info,
    };

    if (vkCreateDescriptorSetLayout(c->dev, &layout_create_info, NULL,
                                    &c->texture_manager.layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create descriptor layout");
        return false;
    }

    c->texture_manager.index = 0;
    u32 counts[FRAMES_IN_FLIGHT];

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        counts[i] = MAX_TEXTURES;
    }

    VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = FRAMES_IN_FLIGHT,
        .pDescriptorCounts = counts,
    };

    // to allocate all in one call
    VkDescriptorSetLayout layouts[] = {
        c->texture_manager.layout,
        c->texture_manager.layout,
        c->texture_manager.layout,
    };

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &count_info,
        .descriptorPool = c->texture_manager.pool,
        .descriptorSetCount = FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(c->dev, &alloc_info,
                                 c->texture_manager.sets) != VK_SUCCESS) {
        LOGM(ERROR, "failed to allocate descriptor sets");
        return false;
    }

    return true;
}

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers) {
    if (!create_instance(rctx, n_exts, exts, n_layers, layers)) {
        return false;
    }

    LOGM(INFO, "created instance");

    struct api_version version = get_api_version();
    LOGM(API_DUMP, "api version: %u.%u.%u", version.major, version.minor,
         version.patch);

    if (!create_db_messenger(rctx)) {
        return false;
    }

    LOGM(INFO, "created debug messenger");

    if (!create_surface(rctx, window)) {
        return false;
    }

    LOGM(INFO, "created surface");

    if (!create_phy_dev(rctx)) {
        return false;
    }

    LOGM(INFO, "created physical device");

    if (!create_log_dev(rctx)) {
        return false;
    }

    LOGM(INFO, "created logical device");

    if (!create_vma(rctx)) {
        return false;
    }

    LOGM(INFO, "created vma");

    i32 w, h;
    glfwGetWindowSize(window, &w, &h);

    if (!create_swapchain(rctx, (u32)w, (u32)h)) {
        return false;
    }

    LOGM(INFO, "created swapchain");

    if (!create_sampler(rctx)) {
        return false;
    }

    LOGM(INFO, "created sampler");

    if (!create_texture_manager(rctx)) {
        return false;
    }

    LOGM(INFO, "created texture manager");

    if (!create_model_color_pipeline(rctx)) {
        return false;
    }

    LOGM(INFO, "created model_color pipeline");

    if (!create_model_texture_pipeline(rctx)) {
        return false;
    }

    LOGM(INFO, "created model_texture pipeline");

    if (!create_frame_data(rctx)) {
        return false;
    }

    LOGM(INFO, "created frame data");

    // staring value
    rctx->render_queue.count = 0;
    rctx->render_queue.capacity = 4;
    rctx->render_queue.cmds =
        malloc(sizeof(struct draw_cmd) * rctx->render_queue.capacity);

    // camera
    rctx->cam.pos = (vec3){0.0f, 0.0f, 0.0f};
    rctx->cam.direction = (vec3){0.0f, 0.0f, 1.0f};
    rctx->cam.speed = 5.0f;
    rctx->cam.sensitivity = 0.15f;

    // models
    rctx->models = darrayCreate(struct model);
    rctx->box_id = renderer_create_model(rctx, "assets/models/box.glb");

    return true;
}

static void push_draw_cmd(struct rcontext *c, struct draw_cmd *cmd) {
    struct render_queue *q = &c->render_queue;

    if (q->count + 1 > q->capacity) {
        u32 new_cap = q->capacity * 2;
        struct draw_cmd *new_data =
            realloc(q->cmds, sizeof(struct draw_cmd) * new_cap);
        if (!new_data) {
            LOGM(WARN, "failed to reallocate draw cmds");
            return;
        }

        q->capacity = new_cap;
        q->cmds = new_data;
    }

    memcpy(&q->cmds[q->count++], cmd, sizeof(struct draw_cmd));
}

void renderer_push_box(struct rcontext *c, vec3 pos, vec3 scale, vec4 color,
                       texture_id texture) {
    struct draw_cmd cmd;
    if (texture == NO_TEXTURE) {
        cmd = (struct draw_cmd){
            .type = DRAW_CMD_TYPE_MODEL_COLOR,
            .pos = pos,
            .scale = scale,
            .model_color.id = c->box_id,
            .model_color.color = color,
        };
    } else {
        cmd = (struct draw_cmd){
            .type = DRAW_CMD_TYPE_MODEL_TEXTURE,
            .pos = pos,
            .scale = scale,
            .model_texture.id = c->box_id,
            .model_texture.texture = texture,
        };
    }

    push_draw_cmd(c, &cmd);
}

void renderer_push_model_color(struct rcontext *c, vec3 pos, vec3 scale,
                               vec4 color, model_id model) {
    struct draw_cmd cmd = (struct draw_cmd){
        .type = DRAW_CMD_TYPE_MODEL_COLOR,
        .pos = pos,
        .scale = scale,
        .model_color.color = color,
        .model_color.id = model,
    };

    push_draw_cmd(c, &cmd);
}

void renderer_push_model_texture(struct rcontext *c, vec3 pos, vec3 scale,
                                 model_id model) {
    struct draw_cmd cmd = (struct draw_cmd){
        .type = DRAW_CMD_TYPE_MODEL_TEXTURE,
        .pos = pos,
        .scale = scale,
        .model_texture.id = model,
        .model_texture.texture = NO_TEXTURE, // model has texture
    };

    push_draw_cmd(c, &cmd);
}

void render_draw_cmds(struct rcontext *c, struct frame_data *data) {
    struct render_queue *q = &c->render_queue;

    f32 aspect =
        (f32)c->swapchain.extent.width / (f32)c->swapchain.extent.height;
    matrix perspective = math_matrix_get_perspective(50, aspect, 0.1f, 100.0f);

    matrix view = math_matrix_look_at(
        c->cam.pos, math_vec3_add(c->cam.pos, c->cam.direction),
        (vec3){0.0f, 1.0f, 0.0f});

    for (u32 i = 0; i < q->count; i++) {
        struct draw_cmd *cmd = &q->cmds[i];

        matrix translate_m =
            math_matrix_translate(cmd->pos.x, cmd->pos.y, cmd->pos.z);
        matrix scale_m =
            math_matrix_scale(cmd->scale.x, cmd->scale.y, cmd->scale.z);
        matrix model = math_matrix_mul(translate_m, scale_m);

        matrix perspective_view = math_matrix_mul(perspective, view);
        matrix m = math_matrix_mul(perspective_view, model);

        switch (cmd->type) {
        case DRAW_CMD_TYPE_MODEL_COLOR: {
            vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              c->model_color_pip.handle);

            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(
                data->cmd_buffer, 0, 1,
                &c->models[cmd->model_color.id].vertex_buffer.handle, offsets);
            vkCmdBindIndexBuffer(
                data->cmd_buffer,
                c->models[cmd->model_color.id].index_buffer.handle, 0,
                VK_INDEX_TYPE_UINT16);

            struct model_color_pc push_constant = {
                .mat = m, // matrix
                .color = cmd->model_color.color,
            };

            vkCmdPushConstants(data->cmd_buffer, c->model_color_pip.layout,
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(struct model_color_pc), &push_constant);
            vkCmdDrawIndexed(data->cmd_buffer,
                             c->models[cmd->model_color.id].n_index, 1, 0, 0,
                             0);
        } break;
        case DRAW_CMD_TYPE_MODEL_TEXTURE: {
            // TODO: only for now just exit
            if (c->models[cmd->model_texture.id].texture == NO_TEXTURE &&
                cmd->model_texture.texture == NO_TEXTURE) {
                LOGM(ERROR, "render model has no texture");
                exit(1);
            }

            texture_id texture = NO_TEXTURE;
            if (c->models[cmd->model_texture.id].texture != NO_TEXTURE) {
                texture = c->models[cmd->model_texture.id].texture;
            }
            if (cmd->model_texture.texture != NO_TEXTURE) {
                if (texture != NO_TEXTURE) {
                    LOGM(WARN,
                         "model_texture: %d has two textures one in the model"
                         "and one extern",
                         cmd->model_texture.id);
                }
                texture = cmd->model_texture.texture;
            }

            vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              c->model_texture_pip.handle);

            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(
                data->cmd_buffer, 0, 1,
                &c->models[cmd->model_texture.id].vertex_buffer.handle,
                offsets);
            vkCmdBindIndexBuffer(
                data->cmd_buffer,
                c->models[cmd->model_texture.id].index_buffer.handle, 0,
                VK_INDEX_TYPE_UINT16);

            struct model_texture_pc push_constant = {
                .mat = m, // matrix
                .texture_index = texture,
            };

            vkCmdPushConstants(
                data->cmd_buffer, c->model_texture_pip.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(struct model_texture_pc), &push_constant);

            vkCmdBindDescriptorSets(
                data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                c->model_texture_pip.layout, 0, 1,
                &c->texture_manager.sets[c->frame_idx], 0, NULL);

            vkCmdDrawIndexed(data->cmd_buffer,
                             c->models[cmd->model_texture.id].n_index, 1, 0, 0,
                             0);
        }; break;
        }
    }
    q->count = 0;
}

static bool record_cmd_buffer(struct rcontext *c) {
    struct frame_data *data = &c->frame_data[c->frame_idx];

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(data->cmd_buffer, &begin_info) != VK_SUCCESS) {
        LOGM(ERROR, "failed to begin recording in the command buffer: %d",
             c->frame_idx);
        return false;
    }

    VkImageMemoryBarrier mem_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = c->swapchain.imgs[c->img_idx],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         NULL, 0, NULL, 1, &mem_barrier);

    mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = c->swapchain.depth_images[c->img_idx].handle,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL,
                         0, NULL, 1, &mem_barrier);

    VkViewport viewport = {
        .width = c->swapchain.extent.width,
        .height = c->swapchain.extent.height,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .extent = c->swapchain.extent,
        .offset = (VkOffset2D){0, 0},
    };

    vkCmdSetViewport(data->cmd_buffer, 0, 1, &viewport);
    vkCmdSetScissor(data->cmd_buffer, 0, 1, &scissor);

    VkClearValue clear_color = {
        .color = {{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = c->swapchain.imgs_views[c->img_idx],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
    };

    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = c->swapchain.depth_images[c->img_idx].view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .depthStencil = {1.0f, 0.0f},
            },
    };

    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info,
        .renderArea =
            {
                .extent = c->swapchain.extent,
                .offset = (VkOffset2D){0, 0},
            },
    };

    vkCmdBeginRendering(data->cmd_buffer, &render_info);

    render_draw_cmds(c, data);

    vkCmdEndRendering(data->cmd_buffer);

    mem_barrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = c->swapchain.imgs[c->img_idx],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(data->cmd_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                         NULL, 1, &mem_barrier);

    if (vkEndCommandBuffer(data->cmd_buffer) != VK_SUCCESS) {
        LOGM(ERROR, "failed to end recording in the command buffer: %d",
             c->frame_idx);
        return false;
    }

    return true;
}

bool renderer_resize(struct rcontext *c, u32 w, u32 h) {
    vkDeviceWaitIdle(c->dev);
    destroy_swapchain(c);
    create_swapchain(c, w, h);
    return true;
}

bool renderer_draw(struct rcontext *c, GLFWwindow *window) {
    struct frame_data *data = &c->frame_data[c->frame_idx];

    vkWaitForFences(c->dev, 1, &data->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(c->dev, 1, &data->in_flight_fence);

    VkResult res = vkAcquireNextImageKHR(c->dev, c->swapchain.handle,
                                         UINT64_MAX, data->image_available,
                                         VK_NULL_HANDLE, &c->img_idx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        i32 w, h;
        glfwGetWindowSize(window, &w, &h);

        renderer_resize(c, (u32)w, (u32)h);
        return true; // no error
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        LOGM(ERROR, "failed to acquire swapchain image");
        return false;
    }

    VkSemaphore finished = c->finished[c->img_idx];

    vkResetCommandBuffer(data->cmd_buffer, 0);
    record_cmd_buffer(c);

    VkSemaphore wait_semaphors[] = {
        data->image_available,
    };

    VkSemaphore signal_semphors[] = {
        finished,
    };

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &data->cmd_buffer,
        .waitSemaphoreCount = ARRAY_COUNT(wait_semaphors),
        .pWaitSemaphores = wait_semaphors,
        .pWaitDstStageMask = wait_stages,
        .signalSemaphoreCount = ARRAY_COUNT(signal_semphors),
        .pSignalSemaphores = signal_semphors,
    };

    if (vkQueueSubmit(c->graphics_queue.handle, 1, &sub_info,
                      data->in_flight_fence) != VK_SUCCESS) {
        LOGM(ERROR, "failed to submit graphics queue");
        return false;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &c->swapchain.handle,
        .pImageIndices = &c->img_idx,
        .waitSemaphoreCount = ARRAY_COUNT(signal_semphors),
        .pWaitSemaphores = signal_semphors,
    };

    if (vkQueuePresentKHR(c->graphics_queue.handle, &present_info) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to present graphics queue");
        return false;
    }

    c->frame_idx = (c->frame_idx + 1) % FRAMES_IN_FLIGHT;

    return true;
}

void renderer_update_cam(struct rcontext *c, GLFWwindow *window, f32 dt) {
    struct camera *cam = &c->cam;

    vec3 forward =
        math_vec3_norm((vec3){cam->direction.x, 0.0f, cam->direction.z});
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 right = math_vec3_norm(math_vec3_cross(cam->direction, up));

    if (glfwGetKey(window, GLFW_KEY_W) != GLFW_RELEASE) {
        cam->pos = math_vec3_subtract(
            cam->pos, math_vec3_scale(forward, cam->speed * dt));
    }
    if (glfwGetKey(window, GLFW_KEY_S) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(forward, cam->speed * dt));
    }
    if (glfwGetKey(window, GLFW_KEY_A) != GLFW_RELEASE) {
        cam->pos = math_vec3_subtract(cam->pos,
                                      math_vec3_scale(right, cam->speed * dt));
    }
    if (glfwGetKey(window, GLFW_KEY_D) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(right, cam->speed * dt));
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_add(cam->pos, math_vec3_scale(up, cam->speed * dt));
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_RELEASE) {
        cam->pos =
            math_vec3_subtract(cam->pos, math_vec3_scale(up, cam->speed * dt));
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (!c->cam.invis_cursor) {
            f64 x, y;
            glfwGetCursorPos(window, &x, &y);

            vec2 pos = (vec2){(f32)x, (f32)y};
            c->cam.last_mouse = pos;
            c->cam.invis_cursor = true;
        }
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        c->cam.invis_cursor = false;
    }

    f64 x, y;
    glfwGetCursorPos(window, &x, &y);

    vec2 pos = (vec2){(f32)x, (f32)y};
    vec2 delta = math_vec2_subtract(pos, c->cam.last_mouse);

    if (c->cam.invis_cursor) {
        c->cam.yaw += delta.x * c->cam.sensitivity;
        c->cam.pitch += delta.y * c->cam.sensitivity;

        math_clamp(c->cam.pitch, -89.0f, 89.0f);

        c->cam.last_mouse = pos;
    }

    vec3 front;
    front.x = cos(DEG2RAD(c->cam.pitch)) * sin(DEG2RAD(c->cam.yaw));
    front.y = sin(DEG2RAD(c->cam.pitch));
    front.z = cos(DEG2RAD(c->cam.pitch)) * cos(DEG2RAD(c->cam.yaw));
    c->cam.direction = math_vec3_norm(front);
}

// bpp always == 4
// internal helper function
static texture_id create_texture(struct rcontext *c, u32 width, u32 height,
                                 void *data) {
    struct texture res = {0};
    create_image(c, &res.image, width, height, VK_FORMAT_R8G8B8A8_SRGB,
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    upload_data_to_image(c, &res.image, width * height * 4, data, width, height,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_SHADER_READ_BIT);

    texture_id id = c->texture_manager.index;
    c->texture_manager.textures[c->texture_manager.index++] = res;

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = res.image.view,
        .sampler = c->sampler,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    };

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        write.dstSet = c->texture_manager.sets[i];

        vkUpdateDescriptorSets(c->dev, 1, &write, 0, NULL);
    }

    return id;
}

texture_id renderer_create_texture(struct rcontext *rctx,
                                   const char *filepath) {
    i32 width, height, bpp;
    u8 *data = stbi_load(filepath, &width, &height, &bpp, 4);
    if (!data) {
        LOGM(ERROR, "failed to load texture: %s", filepath);
        return NO_TEXTURE;
    }

    texture_id id = create_texture(rctx, width, height, data);
    stbi_image_free(data);
    return id;
}

void renderer_destroy_texture(struct rcontext *c, texture_id id) {
    if (id >= (i32)c->texture_manager.index) {
        LOGM(ERROR, "texture id is not valid");
        return;
    }

    struct texture *t = &c->texture_manager.textures[id];

    destroy_image(c, &t->image);
    t->valid = false;
}

static void destroy_texture_manager(struct rcontext *c) {
    vkDestroyDescriptorSetLayout(c->dev, c->texture_manager.layout, NULL);
    vkDestroyDescriptorPool(c->dev, c->texture_manager.pool, NULL);

    for (u32 i = 0; i < c->texture_manager.index; i++) {
        if (c->texture_manager.textures[i].valid) {
            renderer_destroy_texture(c, i);
        }
    }
}

static void fill_buffer(u32 input_stride, void *input_data, u32 output_stride,
                        void *output_data, u32 n_elements, u32 element_size) {
    u8 *output = output_data;
    u8 *input = input_data;

    for (u32 i = 0; i < n_elements; i++) {
        for (u32 j = 0; j < element_size; j++) {
            output[j] = input[j];
        }
        output += output_stride;
        input += input_stride;
    }
}

model_id renderer_create_model(struct rcontext *c, const char *filepath) {
    struct model model = {0};
    model.valid = true;
    model.texture = NO_TEXTURE;

    cgltf_options options = {0};
    cgltf_data *data = NULL;

    cgltf_result error = cgltf_parse_file(&options, filepath, &data);
    if (error != cgltf_result_success) {
        LOGM(ERROR, "failed to load gltf model: %s", filepath);
        return NO_MODEL;
    }

    // TODO: path
    error = cgltf_load_buffers(&options, data, "assets/models");
    if (error != cgltf_result_success) {
        LOGM(ERROR, "failed to load gltf model bufffers: %s", filepath);
        cgltf_free(data);
        return NO_MODEL;
    }

    // check for unsupported model (that my program cant handle rn xD)
    cgltf_primitive *p = &data->meshes[0].primitives[0];
    assert(data->meshes_count == 1);
    assert(data->meshes[0].primitives_count == 1);
    assert(p->attributes_count > 0);
    assert(p->indices->component_type == cgltf_component_type_r_16u);
    assert(p->indices->stride == sizeof(u16));

    // index buffer
    u8 *buffer_base = (u8 *)p->indices->buffer_view->buffer->data;
    u64 index_data_size = p->indices->buffer_view->size;
    void *index_data = buffer_base + p->indices->buffer_view->offset;

    model.index_buffer = create_device_local_buffer(
        c, index_data_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    upload_data_to_buffer(c, &model.index_buffer, index_data_size, index_data);
    model.n_index = p->indices->count;

    // vertex buffer
    // pos (3) uv (2) normals (3)
    u32 output_stride = sizeof(f32) * 8;
    u32 n_vertex = p->attributes->data->count;
    u32 vertex_data_size = output_stride * n_vertex;
    u8 *vertex_data = malloc(vertex_data_size);

    for (u32 i = 0; i < p->attributes_count; i++) {
        cgltf_attribute *a = p->attributes + i;
        buffer_base = (u8 *)a->data->buffer_view->buffer->data;
        u32 input_stride = a->data->stride;

        if (a->type == cgltf_attribute_type_position) {
            void *pos_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, pos_data, output_stride, vertex_data,
                        n_vertex, sizeof(f32) * 3);
        }

        else if (a->type == cgltf_attribute_type_texcoord) {
            void *texcoord_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, texcoord_data, output_stride,
                        vertex_data + (sizeof(f32) * 3), n_vertex,
                        sizeof(f32) * 2);
        }

        else if (a->type == cgltf_attribute_type_normal) {
            void *normal_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, normal_data, output_stride,
                        vertex_data + (sizeof(f32) * 5), n_vertex,
                        sizeof(f32) * 3);
        }
    }

    model.vertex_buffer = create_device_local_buffer(
        c, vertex_data_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    upload_data_to_buffer(c, &model.vertex_buffer, vertex_data_size,
                          vertex_data);

    // texture
    if (data->materials_count != 1) {
        LOGM(WARN, "model has no texture: %s", filepath);
        goto end;
    }

    cgltf_material *material = &data->materials[0];
    if (!material->has_pbr_metallic_roughness) {
        LOGM(WARN, "model has no texture: %s", filepath);
        goto end;
    }

    cgltf_texture_view albedo_texture_view =
        material->pbr_metallic_roughness.base_color_texture;

    if (albedo_texture_view.has_transform ||
        albedo_texture_view.texcoord != 0 || !albedo_texture_view.texture) {
        LOGM(WARN, "model has no texture: %s", filepath);
        goto end;
    }

    cgltf_texture *albedo_texture = albedo_texture_view.texture;

    cgltf_buffer_view *buffer_view = albedo_texture->image->buffer_view;

    if (buffer_view->size >= INT32_MAX) {
        LOGM(WARN, "model has no texture: %s", filepath);
        goto end;
    }

    u8 *data_ptr = (u8 *)buffer_view->buffer->data + buffer_view->offset;

    i32 bpp, width, height;
    u8 *texture_data = stbi_load_from_memory(
        (stbi_uc *)data_ptr, buffer_view->size, &width, &height, &bpp, 4);
    if (!texture_data) {
        LOGM(ERROR, "failed to load model texture: %s", filepath);
        return NO_MODEL;
    }

    bpp = 4;

    model.texture = create_texture(c, width, height, texture_data);
    if (model.texture == NO_TEXTURE) {
        LOGM(ERROR, "failed to create model texture: %s", filepath);
    }

    stbi_image_free(texture_data);

end:

    cgltf_free(data);

    model_id id = darrayLength(c->models);
    darrayPush(c->models, model);

    return id;
}

void renderer_destroy_model(struct rcontext *c, model_id id) {
    if (id >= (i32)darrayLength(c->models)) {
        LOGM(ERROR, "inavlid model id");
        return;
    }

    struct model *model = &c->models[id];

    vmaDestroyBuffer(c->allocator, model->vertex_buffer.handle,
                     model->vertex_buffer.alloc);
    vmaDestroyBuffer(c->allocator, model->index_buffer.handle,
                     model->index_buffer.alloc);
    *model = (struct model){0};
}

bool renderer_set_model_texture(struct rcontext *c, model_id model,
                                texture_id texture) {
    if (model >= (i32)darrayLength(c->models)) {
        LOGM(ERROR, "invalid model index: %d", model);
        return false;
    }

    if (texture >= (i32)c->texture_manager.index) {
        LOGM(ERROR, "inavlid texture index: %d", texture);
        return false;
    }

    struct model *m = &c->models[model];
    if (m->texture != NO_TEXTURE) {
        LOGM(API_DUMP, "overwriting model texture");
    }

    m->texture = texture;

    return true;
}

void renderer_deint(struct rcontext *rctx) {
    vkDeviceWaitIdle(rctx->dev);

    vkDestroySampler(rctx->dev, rctx->sampler, NULL);

    for (u32 i = 0; i < darrayLength(rctx->models); i++) {
        if (rctx->models[i].valid) {
            renderer_destroy_model(rctx, i);
        }
    }

    darrayDestroy(rctx->models);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(rctx->dev, rctx->frame_data[i].image_available,
                           NULL);
        vkDestroySemaphore(rctx->dev, rctx->finished[i], NULL);
        vkDestroyFence(rctx->dev, rctx->frame_data[i].in_flight_fence, NULL);
    }

    vkDestroyCommandPool(rctx->dev, rctx->cmd_pool, NULL);

    destroy_pipeline(rctx, &rctx->model_color_pip);
    destroy_pipeline(rctx, &rctx->model_texture_pip);

    destroy_texture_manager(rctx);

    destroy_swapchain(rctx);
    vkDestroySurfaceKHR(rctx->instance, rctx->surface, NULL);

    vmaDestroyAllocator(rctx->allocator);
    vkDestroyDevice(rctx->dev, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            rctx->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(rctx->instance, rctx->db_messenger, NULL);
    vkDestroyInstance(rctx->instance, NULL);
}
