#include "renderer.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace Engine;

Renderer::Renderer()
	: w_width(800), w_height(600), w_title("Vulkan Application"), isRunning(true), _window(nullptr), _instance(VK_NULL_HANDLE),
	  _surface(VK_NULL_HANDLE) {}

Renderer::~Renderer() {}

void Renderer::create() {

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

	VkApplicationInfo app_info = {
			.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext				= nullptr,
			.pApplicationName	= "VkRenderer",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName		= "No Engine",
			.engineVersion		= VK_MAKE_VERSION(1, 0, 0),
			.apiVersion			= VK_API_VERSION_1_4,
	};

	VkInstanceCreateInfo create_info = {
			.sType					 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext					 = nullptr,
			.flags					 = VkInstanceCreateFlags(NULL),
			.pApplicationInfo		 = &app_info,
			.enabledExtensionCount	 = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
	};

	if (enableValidation) {
		create_info.enabledLayerCount	= static_cast<uint32_t>(validationLayers.size());
		create_info.ppEnabledLayerNames = validationLayers.data();
	}

	VkResult result = vkCreateInstance(&create_info, nullptr, &_instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error(std::string("vkCreateInstance failed: ") + std::to_string(result));
	}

	if (!SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface)) {
		throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
	}
	std::cout << "Surface created successfully" << std::endl;
}

void Renderer::destroy() {
	vkDestroySurfaceKHR(_instance, _surface, nullptr);
	vkDestroyInstance(_instance, nullptr);
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
	Renderer::destroy();
}
