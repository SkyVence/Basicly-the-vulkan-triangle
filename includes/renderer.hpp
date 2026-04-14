#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

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
		Renderer();
		~Renderer();
		void run();
		void create();
		void destroy();

	  private:
		// SDL
		SDL_Window* _window;
		SDL_Event	_event;

		// Vulkan
		VkInstance			 _instance;
		VkInstanceCreateInfo _create_info;
		VkSurfaceKHR		 _surface = VK_NULL_HANDLE;
	};
} // namespace Engine
