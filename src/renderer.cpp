#include "renderer.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Engine;

static const std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

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

Renderer::Renderer() : w_width(1280), w_height(720), w_title("Vulkan Application"), isRunning(true), _window(nullptr) {
	create();
	pickPhysicalDevice();
}

Renderer::~Renderer() { destroy(); }

void Renderer::create() {

	try {

		if (!SDL_Init(SDL_INIT_VIDEO)) {
			throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
		}

		_window = SDL_CreateWindow(w_title.c_str(), w_width, w_height, SDL_WINDOW_VULKAN);
		if (!_window) {
			throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
		}

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
	auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const& physicalDevice) { return isDeviceSuitable(physicalDevice); });
	if (devIter == physicalDevices.end()) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
	_physicalDevice.emplace(*devIter);
	std::cout << "Physical device selected: " << _physicalDevice->getProperties().deviceName << std::endl;
}

void Renderer::destroy() {
	// RAII handles cleanup automatically
	_surface.reset();
	_instance.reset();
	SDL_DestroyWindow(_window);
	SDL_Quit();
}

void Renderer::run() {
	while (isRunning) {
		if (SDL_PollEvent(&_event)) {
			if (_event.type == SDL_EVENT_QUIT) {
				isRunning = false;
			}
		}
	}
}
