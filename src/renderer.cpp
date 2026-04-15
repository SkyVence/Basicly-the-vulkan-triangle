#include "renderer.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_raii.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Engine;

static const std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

// Helper function to rank a device (higher score is better)
// Discrete GPUs get highest priority, followed by integrated GPUs
static int rankDevice(vk::raii::PhysicalDevice const& physicalDevice) {
	auto props = physicalDevice.getProperties();

	switch (props.deviceType) {
	case vk::PhysicalDeviceType::eDiscreteGpu:
		return 1000;
	case vk::PhysicalDeviceType::eIntegratedGpu:
		return 500;
	case vk::PhysicalDeviceType::eVirtualGpu:
		return 100;
	case vk::PhysicalDeviceType::eCpu:
		return 10;
	default:
		return 0;
	}
}

// Helper function to get device type as string
static std::string getDeviceTypeString(vk::PhysicalDeviceType type) {
	switch (type) {
	case vk::PhysicalDeviceType::eDiscreteGpu:
		return "Discrete GPU";
	case vk::PhysicalDeviceType::eIntegratedGpu:
		return "Integrated GPU";
	case vk::PhysicalDeviceType::eVirtualGpu:
		return "Virtual GPU";
	case vk::PhysicalDeviceType::eCpu:
		return "CPU";
	default:
		return "Unknown";
	}
}

bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice) {
	// Check if the physicalDevice supports the Vulkan 1.3 API version
	bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

	// Check if any of the queue families support graphics operations
	auto queueFamilies	  = physicalDevice.getQueueFamilyProperties();
	bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

	// Check if all required physicalDevice extensions are available
	auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
	bool supportsAllRequiredExtensions =
			std::ranges::all_of(requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
				return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension) {
					return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
				});
			});

	// Check if the physicalDevice supports the required features (dynamic rendering and extended dynamic state)
	auto features				  = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
																		 vk::PhysicalDeviceVulkan13Features,
																		 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
	bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
									features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

	// Return true if the physicalDevice meets all the criteria
	return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

Renderer::Renderer(SDL_Window* window, SDL_Event* event) : isRunning(true), _window(window), _event(event) {
	create();
	pickPhysicalDevice();
	createLogicalDevice();
}

Renderer::~Renderer() { destroy(); }

void Renderer::create() {

	try {
		// Get SDL Vulkan extensions
		uint32_t		   sdlExtCount = 0;
		const char* const* sdlExts	   = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
		if (sdlExts == nullptr) {
			throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
		}

		std::vector<const char*> extensions(sdlExts, sdlExts + sdlExtCount);

		std::cout << "Enabled extensions:" << std::endl;
		for (const char* ext : extensions) {
			std::cout << "  " << ext << std::endl;
		}

		vk::ApplicationInfo app_info("VkRenderer", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), vk::ApiVersion14);

		vk::InstanceCreateInfo create_info({},
										   &app_info,
										   enableValidation ? static_cast<uint32_t>(validationLayers.size()) : 0,
										   enableValidation ? validationLayers.data() : nullptr,
										   static_cast<uint32_t>(extensions.size()),
										   extensions.data());

		// Create Vulkan context and instance using RAII
		_instance.emplace(_context, create_info);

		// Create surface using SDL
		VkSurfaceKHR rawSurface;
		vk::Instance vkInstance = *_instance; // Get vk::Instance from vk::raii::Instance
		if (!SDL_Vulkan_CreateSurface(_window, vkInstance, nullptr, &rawSurface)) {
			throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
		}

		// Wrap the raw surface in RAII
		_surface.emplace(*_instance, rawSurface);

		std::cout << "Surface created successfully" << std::endl;

	} catch (const vk::SystemError& err) {
		throw std::runtime_error(std::string("Vulkan system error: ") + err.what());
	} catch (const std::exception& err) {
		throw std::runtime_error(std::string("Standard exception: ") + err.what());
	}
}

void Renderer::pickPhysicalDevice() {
	std::vector<vk::raii::PhysicalDevice> physicalDevices = _instance->enumeratePhysicalDevices();

	int bestIndex = -1;
	int bestScore = -1;

	// Find the suitable device with the highest score (discrete GPU preferred)
	for (size_t i = 0; i < physicalDevices.size(); ++i) {
		if (isDeviceSuitable(physicalDevices[i])) {
			int score = rankDevice(physicalDevices[i]);
			if (score > bestScore) {
				bestScore = score;
				bestIndex = static_cast<int>(i);
			}
		}
	}

	if (bestIndex == -1) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	_physicalDevice.emplace(physicalDevices[bestIndex]);
	auto props		 = _physicalDevice->getProperties();
	auto memoryProps = _physicalDevice->getMemoryProperties();
	std::cout << "Physical device selected: " << props.deviceName << " (" << getDeviceTypeString(props.deviceType)
			  << ") VRAM: " << memoryProps.memoryHeaps[0].size / 1024 / 1024 << " MB" << std::endl;
}

void Renderer::createLogicalDevice() {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice->getQueueFamilyProperties();

	auto graphicsQueueFamilyProperty = std::ranges::find_if(
			queueFamilyProperties, [](auto const& qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });

	auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

	float					  queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsIndex, 1, &queuePriority);

	// Create a chain of feature structures
	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain;
	featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering					   = true;
	featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = true;
	std::vector<const char*> requiredDeviceExtension										   = {vk::KHRSwapchainExtensionName};
	vk::DeviceCreateInfo	 deviceCreateInfo(
			{}, 1, &deviceQueueCreateInfo, static_cast<uint32_t>(requiredDeviceExtension.size()), requiredDeviceExtension.data());
	deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();

	_device.emplace(_physicalDevice.value(), deviceCreateInfo);
	_graphicsQueue.emplace(_device.value().getQueue(graphicsIndex, 0));
	std::cout << "Graphics queue created" << std::endl;
}

void Renderer::destroy() {
	_graphicsQueue.reset();
	_device.reset();
	_physicalDevice.reset();
	_surface.reset();
	_instance.reset();
	SDL_DestroyWindow(_window);
	SDL_Quit();
}

void Renderer::run() {
	while (isRunning) {
		if (SDL_PollEvent(_event)) {
			if (_event->type == SDL_EVENT_QUIT) {
				isRunning = false;
			}
		}
	}
}
