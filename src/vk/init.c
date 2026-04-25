
#include "types.h"
#include <log.h>
#include <vk/init.h>
#include <window.h>

#include <vulkan/vulkan.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

struct vk_api_version init_get_api_version() {
    u32 instance_version;
    if (vkEnumerateInstanceVersion(&instance_version) != VK_SUCCESS) {
        LOGM(ERROR, "failed to get instance version");
        exit(1);
    }

    return (struct vk_api_version){
        .major = VK_API_VERSION_MAJOR(instance_version),
        .minor = VK_API_VERSION_MINOR(instance_version),
        .patch = VK_API_VERSION_PATCH(instance_version),
    };
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

static bool create_db_messenger(struct vk_init *init) {
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
            init->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!vkCreateDebugUtilsMessengerEXT) {
        LOGM(ERROR, "failed to load debug utils messenger function pointer");
        return false;
    }

    if (vkCreateDebugUtilsMessengerEXT(init->instance, &create_info, NULL,
                                       &init->db_messenger) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create debug messenger");
        return false;
    }

    return true;
}

static bool create_instance(VkInstance *instance, i32 n_exts, const char **exts,
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

    if (vkCreateInstance(&create_info, NULL, instance) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create instance");
        return false;
    }

    return true;
}

static bool create_surface(struct vk_init *init, struct window *window) {
    window_create_surface(window, init);
    if (!init->surface) {
        LOGM(ERROR, "failed to create surface");
        return false;
    }

    return true;
}

static bool create_phy_dev(struct vk_init *init) {
    u32 n_phys_dev;
    vkEnumeratePhysicalDevices(init->instance, &n_phys_dev, NULL);
    if (n_phys_dev == 0) {
        LOGM(ERROR, "failed to find GPU which supports vulkan");
        return false;
    }

    // no one has more than 8 gpus :sob:
    VkPhysicalDevice devs[8];
    vkEnumeratePhysicalDevices(init->instance, &n_phys_dev, devs);

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
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, j, init->surface,
                                                 &supported);
            if (supported) {
                present_queue_family_index = j;
                if (graphics_queue_family_index != -1)
                    break;
            }
        }

        if (graphics_queue_family_index != -1 &&
            present_queue_family_index != -1) {

            init->phy_dev = devs[i];
            init->present_queue.index = present_queue_family_index;
            init->graphics_queue.index = graphics_queue_family_index;

            break;
        } else if (i == n_phys_dev - 1) {
            LOGM(ERROR, "failed to find sutable GPU");
            return false;
        }
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(init->phy_dev, &props);
    LOGM(INFO, "picked GPU: %s", props.deviceName);

    return true;
}

static bool create_log_dev(struct vk_init *init) {
    VkDeviceQueueCreateInfo queue_infos[2];

    float priority = 1.0f;

    u32 n_queues = 0;
    queue_infos[n_queues++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = init->graphics_queue.index,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    if (init->graphics_queue.index != init->present_queue.index) {
        queue_infos[n_queues++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = init->present_queue.index,
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

    if (vkCreateDevice(init->phy_dev, &create_info, NULL, &init->dev) !=
        VK_SUCCESS) {
        LOGM(ERROR, "failed to create logical device");
        return false;
    }

    vkGetDeviceQueue(init->dev, init->graphics_queue.index, 0,
                     &init->graphics_queue.handle);
    vkGetDeviceQueue(init->dev, init->present_queue.index, 0,
                     &init->present_queue.handle);

    return true;
}

static bool create_vma(struct vk_init *init) {
    VmaAllocatorCreateInfo create_info = {
        .physicalDevice = init->phy_dev,
        .device = init->dev,
        .instance = init->instance,
    };

    if (vmaCreateAllocator(&create_info, &init->allocator) != VK_SUCCESS) {
        LOGM(ERROR, "failed to create vma");
        return false;
    }

    return true;
}

bool init_vk(struct vk_init *init, struct window *window) {
    memset(init, 0, sizeof(struct vk_init));

    u32 n_window_exts;
    const char **window_exts = window_get_instance_exts(&n_window_exts);

    u32 n_exts = n_window_exts + 1;
    const char *exts[n_exts];

    LOGM(API_DUMP, "window ext count: %d", n_window_exts);
    for (u32 i = 0; i < n_window_exts; i++) {
        exts[i] = window_exts[i];
        LOGM(API_DUMP, "instance extension: %s", window_exts[i]);
    }
    exts[n_window_exts] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    const char *layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };

    if (!create_instance(&init->instance, n_exts, exts, ARRAY_COUNT(layers),
                         layers)) {
        return false;
    }

    LOGM(API_DUMP, "created instance");

    struct vk_api_version version = init_get_api_version();
    LOGM(API_DUMP, "api version: %u.%u.%u", version.major, version.minor,
         version.patch);

    if (!create_db_messenger(init)) {
        return false;
    }

    LOGM(API_DUMP, "created debug messenger");

    if (!create_surface(init, window)) {
        return false;
    }

    LOGM(API_DUMP, "created surface");

    if (!create_phy_dev(init)) {
        return false;
    }

    LOGM(API_DUMP, "created physical device");

    if (!create_log_dev(init)) {
        return false;
    }

    LOGM(API_DUMP, "created logical device");

    if (!create_vma(init)) {
        return false;
    }

    return true;
}

void deinit_vk(struct vk_init *init) {
    vmaDestroyAllocator(init->allocator);
    vkDestroyDevice(init->dev, NULL);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            init->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(init->instance, init->db_messenger, NULL);

    vkDestroyInstance(init->instance, NULL);
}
