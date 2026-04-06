
#include "renderer.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <stdlib.h>

#include "log.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

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

static bool create_swapchain(struct rcontext *c, GLFWwindow *window) {
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

    i32 w, h;
    glfwGetWindowSize(window, &w, &h);

    VkExtent2D extent = (VkExtent2D){
        .width = w,
        .height = h,
    };

    extent.width = MIN(caps.maxImageExtent.width, extent.width);
    extent.height = MIN(caps.maxImageExtent.height, extent.height);
    extent.width = MAX(caps.minImageExtent.width, extent.width);
    extent.height = MAX(caps.maxImageExtent.height, extent.height);

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

    VkShaderModule frag_module;
    if (!create_shader_module(c, &frag_module, "shaders/frag.spv")) {
        return false;
    }

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

bool renderer_init(struct rcontext *rctx, GLFWwindow *window, i32 n_exts,
                   const char **exts, i32 n_layers, const char **layers) {
    if (!create_instance(rctx, n_exts, exts, n_layers, layers)) {
        return false;
    }

    LOGM(INFO, "Created Instance");

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

    if (!create_swapchain(rctx, window)) {
        return false;
    }

    LOGM(INFO, "created swapchain");

    if (!create_pipeline(rctx)) {
        return false;
    }

    LOGM(INFO, "created pipline");

    return true;
}

void renderer_deint(struct rcontext *rctx) {
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
