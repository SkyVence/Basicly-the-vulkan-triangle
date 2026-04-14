#pragma once
#include <vulkan/vulkan_core.h>

class VulkanApplication {
public:
    VulkanApplication() {}
    ~VulkanApplication() {}

    VkInstance _instance{};
    VkDebugUtilsMessengerEXT _debug_messenger{};
    VkPhysicalDevice _gpuDevice{};
    VkDevice _device{};
    VkSurfaceKHR _surface{};

    void init();
    void cleanup();
};
