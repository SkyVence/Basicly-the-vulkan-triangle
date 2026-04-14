#include "vulkanApplication.hpp"
#include <stdexcept>
#include <vulkan/vulkan_core.h>


void VulkanApplication::init() {
    VkApplicationInfo appInfo{};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VKLearn Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void VulkanApplication::cleanup() {
    vkDestroyInstance(_instance, nullptr);
}
