#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
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
		Renderer(SDL_Window* window, SDL_Event* event);
		~Renderer();

		uint32_t	w_width;
		uint32_t	w_height;
		std::string w_title;
		bool		isRunning;

		void	 run();
		void	 destroy();
		uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);

	  private:
		// SDL
		SDL_Window* _window;
		SDL_Event*	_event;

		// Vulkan RAII
		// Member variables must be declared in order of parent-to-child dependency.
		// They are destroyed in reverse order (child-to-parent), so declare context first,
		// then instance, then surface, then physical device.
		vk::raii::Context					_context;
		std::optional<vk::raii::Instance>	_instance;
		std::optional<vk::raii::SurfaceKHR> _surface;

		// Device
		std::optional<vk::raii::PhysicalDevice> _physicalDevice;
		std::optional<vk::raii::Device>			_device;

		// Queue
		std::optional<vk::raii::Queue> _graphicsQueue;
		uint32_t					   _queueIndex = ~0;

		// SwapChain
		std::optional<vk::raii::SwapchainKHR> _swapChain;
		std::vector<vk::Image>				  _swapChainImages;
		vk::SurfaceFormatKHR				  _swapSurfaceFormat;
		vk::PresentModeKHR					  _swapPresentMode;
		vk::Extent2D						  _swapExtent;

		// Pipeline
		vk::raii::PipelineLayout _pipelineLayout   = nullptr;
		vk::raii::Pipeline		 _graphicsPipeline = nullptr;

		// Image View
		std::vector<vk::raii::ImageView> _swapChainImageViews;

		// CommandPool
		vk::raii::CommandPool	_commandPool   = nullptr;
		vk::raii::CommandBuffer _commandBuffer = nullptr;

		void createInstance();
		void createSurface();
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createSwapChain();
		void createImageViews();
		void createGraphicsPipeline();
		void createCommandPool();
		void createCommandBuffer();
		void recordCommandBuffer(uint32_t imageIndex);
		void transition_image_layout(uint32_t				 imageIndex,
									 vk::ImageLayout		 old_layout,
									 vk::ImageLayout		 new_layout,
									 vk::AccessFlags2		 src_access_mask,
									 vk::AccessFlags2		 dst_access_mask,
									 vk::PipelineStageFlags2 src_stage_mask,
									 vk::PipelineStageFlags2 dst_stage_mask);
		void drawFrame();

		vk::SurfaceFormatKHR				 chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats);
		vk::PresentModeKHR					 chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes);
		vk::Extent2D						 chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities);
		[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
	};
} // namespace Engine
