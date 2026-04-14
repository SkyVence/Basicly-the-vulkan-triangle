#include "renderer.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_raii.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Engine;

Renderer::Renderer() : w_width(800), w_height(600), w_title("Vulkan Application"), isRunning(true), _window(nullptr) {}

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

	vk::ApplicationInfo app_info("VkRenderer", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_4);

	vk::InstanceCreateInfo create_info({},
									   &app_info,
									   enableValidation ? static_cast<uint32_t>(validationLayers.size()) : 0,
									   enableValidation ? validationLayers.data() : nullptr,
									   static_cast<uint32_t>(extensions.size()),
									   extensions.data());

	// Create Vulkan context and instance using RAII
	vk::raii::Context context;
	_instance.emplace(context, create_info);

	// Create surface using SDL
	VkSurfaceKHR rawSurface;
	vk::Instance vkInstance = *_instance; // Get vk::Instance from vk::raii::Instance
	if (!SDL_Vulkan_CreateSurface(_window, vkInstance, nullptr, &rawSurface)) {
		throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
	}

	// Wrap the raw surface in RAII
	_surface.emplace(_instance.value(), rawSurface);

	std::cout << "Surface created successfully" << std::endl;
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
	Renderer::destroy();
}
