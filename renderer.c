
#include "renderer.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

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

    for (i32 i = 0; i < n_phys_dev; i++) {
        VkPhysicalDevice dev = devs[i];
        u32 n_queues;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &n_queues, NULL);

        VkQueueFamilyProperties props[8];
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &n_queues, props);

        i32 graphics_queue_family_index = -1;
        i32 present_queue_family_index = -1;

        for (i32 j = 0; j < n_queues; j++) {
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

    return true;
}

void renderer_deint(struct rcontext *rctx) {
    vkDestroySurfaceKHR(rctx->instance, rctx->surface, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            rctx->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(rctx->instance, rctx->db_messenger, NULL);
    vkDestroyInstance(rctx->instance, NULL);
}
