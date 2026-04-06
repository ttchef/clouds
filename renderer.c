
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

    // TODO: actuall check the features of the GPU
    c->phy_dev = devs[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(c->phy_dev, &props);
    LOGM(INFO, "picked GPU: %s", props.deviceName);

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
