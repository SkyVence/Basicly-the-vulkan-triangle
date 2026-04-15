#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace Engine {

	// Validation layers (optional, useful during development)
	static const std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation",
	};

#ifdef NDEBUG
	constexpr bool enableValidation = false;
#else
	constexpr bool enableValidation = true;
#endif

	class Renderer {
	  public:
		uint32_t	w_width;
		uint32_t	w_height;
		std::string w_title;
		bool		isRunning;
		Renderer(SDL_Window* window, SDL_Event* event);
		~Renderer();
		void run();
		void destroy();

	  private:
		// SDL
		SDL_Window* _window;
		SDL_Event*	_event;

		// Vulkan RAII
		// Member variables must be declared in order of parent-to-child dependency.
		// They are destroyed in reverse order (child-to-parent), so declare context first,
		// then instance, then surface, then physical device.
		vk::raii::Context						_context;
		std::optional<vk::raii::Instance>		_instance;
		std::optional<vk::raii::SurfaceKHR>		_surface;
		std::optional<vk::raii::PhysicalDevice> _physicalDevice;
		std::optional<vk::raii::Device>			_device;
		std::optional<vk::raii::Queue>			_graphicsQueue;

		void createInstance();
		void createSurface();
		void pickPhysicalDevice();
		void createLogicalDevice();
	};
} // namespace Engine
