
#include "renderer.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <stdlib.h>
#include <string.h>

#include "log.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

const f32 vertices[] = {
    -0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, -0.5f, 0.0f,
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

    VkPhysicalDeviceDynamicRenderingFeatures dym_rendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = true,
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
    }

    return true;
}

static void destroy_swapchain(struct rcontext *c) {
    if (c->swapchain.imgs) {
        free(c->swapchain.imgs);
    }

    for (u32 i = 0; i < c->swapchain.n_imgs; i++) {
        vkDestroyImageView(c->dev, c->swapchain.imgs_views[i], NULL);
    }

    if (c->swapchain.imgs_views) {
        free(c->swapchain.imgs_views);
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

static bool create_pipeline(struct rcontext *c) {
    // TODO: not hardcoded paths
    VkShaderModule vert_module;
    if (!create_shader_module(c, &vert_module, "shaders/vert.spv")) {
        return false;
    }

    LOGM(API_DUMP, "created vertex shader module");

    VkShaderModule frag_module;
    if (!create_shader_module(c, &frag_module, "shaders/frag.spv")) {
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

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(f32) * 3,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrib_desc = {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    };

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attrib_desc,
    };

    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology =
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // TODO: maybe wrong mode
    };

    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthBoundsTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
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

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    if (vkCreatePipelineLayout(c->dev, &layout_create_info, NULL,
                               &c->pipeline.layout) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create pipeline layout");
        goto error_path;
    }

    LOGM(API_DUMP, "created pipeline layout");

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &dynamic_rendering,
        .layout = c->pipeline.layout,
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
                                  &c->pipeline.handle) != VK_SUCCESS) {
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
                              &c->frame_data[i].finished) != VK_SUCCESS) {
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
                                                VkDeviceSize size) {
    struct buffer res = {0};

    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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

static bool create_buffers(struct rcontext *c) {
    VkDeviceSize vertex_buffer_size = ARRAY_COUNT(vertices) * sizeof(f32);
    struct buffer staging =
        create_staging_buffer(c, vertex_buffer_size, vertices);
    c->vertex_buffer = create_device_local_buffer(c, vertex_buffer_size);
    copy_buffer(c, &staging, &c->vertex_buffer);
    vmaDestroyBuffer(c->allocator, staging.handle, staging.alloc);

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

    i32 w, h;
    glfwGetWindowSize(window, &w, &h);

    if (!create_swapchain(rctx, (u32)w, (u32)h)) {
        return false;
    }

    LOGM(INFO, "created swapchain");

    if (!create_pipeline(rctx)) {
        return false;
    }

    LOGM(INFO, "created pipline");

    if (!create_frame_data(rctx)) {
        return false;
    }

    LOGM(INFO, "created frame data");

    if (!create_vma(rctx)) {
        return false;
    }

    LOGM(INFO, "created vma");

    if (!create_buffers(rctx)) {
        return false;
    }

    LOGM(INFO, "created gpu buffers");

    return true;
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
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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

    vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      c->pipeline.handle);

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
        .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
    };

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = c->swapchain.imgs_views[c->img_idx],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
    };

    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .renderArea =
            {
                .extent = c->swapchain.extent,
                .offset = (VkOffset2D){0, 0},
            },
    };

    vkCmdBeginRendering(data->cmd_buffer, &render_info);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(data->cmd_buffer, 0, 1, &c->vertex_buffer.handle,
                           offsets);

    vkCmdDraw(data->cmd_buffer, 3, 1, 0, 0);

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

    vkResetCommandBuffer(data->cmd_buffer, 0);
    record_cmd_buffer(c);

    VkSemaphore wait_semaphors[] = {
        data->image_available,
    };

    VkSemaphore signal_semphors[] = {
        data->finished,
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

void renderer_deint(struct rcontext *rctx) {
    vkDeviceWaitIdle(rctx->dev);

    vmaDestroyBuffer(rctx->allocator, rctx->vertex_buffer.handle,
                     rctx->vertex_buffer.alloc);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(rctx->dev, rctx->frame_data[i].image_available,
                           NULL);
        vkDestroySemaphore(rctx->dev, rctx->frame_data[i].finished, NULL);
        vkDestroyFence(rctx->dev, rctx->frame_data[i].in_flight_fence, NULL);
    }

    vkDestroyCommandPool(rctx->dev, rctx->cmd_pool, NULL);

    vkDestroyPipelineLayout(rctx->dev, rctx->pipeline.layout, NULL);
    vkDestroyPipeline(rctx->dev, rctx->pipeline.handle, NULL);

    destroy_swapchain(rctx);
    vkDestroySurfaceKHR(rctx->instance, rctx->surface, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            rctx->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(rctx->instance, rctx->db_messenger, NULL);
    vkDestroyInstance(rctx->instance, NULL);
}
