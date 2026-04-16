#include "renderer.hpp"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_raii.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
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
																		 vk::PhysicalDeviceVulkan11Features,
																		 vk::PhysicalDeviceVulkan13Features,
																		 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
	bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
									features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
									features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

	// Return true if the physicalDevice meets all the criteria
	return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

uint32_t Renderer::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities) {
	auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
	if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
		minImageCount = surfaceCapabilities.maxImageCount;
	}
	return minImageCount;
}

vk::SurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
		return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
	});
	return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR Renderer::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes) {
	assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
	return std::ranges::any_of(availablePresentModes, [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
				   ? vk::PresentModeKHR::eMailbox
				   : vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	int width, height;

	SDL_GetWindowSizeInPixels(_window, &width, &height);

	return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

/*
 *
 * Well it's the start of a long journey 15/4/26 11:33PM
 */

Renderer::Renderer(SDL_Window* window, SDL_Event* event) : isRunning(true), _window(window), _event(event) {
	createInstance();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createGraphicsPipeline();
	createCommandPool();
	createCommandBuffer();
}

Renderer::~Renderer() { destroy(); }

void Renderer::createInstance() {
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
		std::cout << "Created Vulkan instance" << std::endl;
	} catch (const vk::SystemError& err) {
		throw std::runtime_error(std::string("Vulkan system error: ") + err.what());
	} catch (const std::exception& err) {
		throw std::runtime_error(std::string("Standard exception: ") + err.what());
	}
}

/*
 * Create a Vulkan surface with a window compatible with the vulkan api.
 */
void Renderer::createSurface() {

	try {

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
/*
 * Choose a suitable physical device from the available devices. Mostly a dedicated GPU is preferred.
 */
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

/*
 * Create a logical device
 */
void Renderer::createLogicalDevice() {
	// Find the index of the first queue family that supports graphics operations
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice->getQueueFamilyProperties();

	// Get the first index into queueFamilyProperties that supports graphics operations and present

	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			_physicalDevice->getSurfaceSupportKHR(qfpIndex, *_surface)) {
			// found a queue family that supports both graphics and present
			_queueIndex = qfpIndex;
			break;
		}
	}
	if (_queueIndex == uint32_t(~0)) {
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// Create a chain of feature structures
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
					   vk::PhysicalDeviceVulkan11Features,
					   vk::PhysicalDeviceVulkan13Features,
					   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain;
	featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering					   = true;
	featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = true;
	featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters				   = true;

	// create a Device
	float					  queuePriority = 0.5f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo =
			vk::DeviceQueueCreateInfo().setQueueFamilyIndex(_queueIndex).setQueueCount(1).setPQueuePriorities(&queuePriority);
	vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
													.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
													.setQueueCreateInfoCount(1)
													.setPQueueCreateInfos(&deviceQueueCreateInfo)
													.setEnabledExtensionCount(static_cast<uint32_t>(requiredDeviceExtension.size()))
													.setPpEnabledExtensionNames(requiredDeviceExtension.data());

	_device.emplace(_physicalDevice.value(), deviceCreateInfo);
	_graphicsQueue.emplace(_device.value(), _queueIndex, 0);
	std::cout << "Created logical device" << std::endl;
}
/*
 * Create a Vulkan SwapChain must be done after the logical device is created.
 */
void Renderer::createSwapChain() {
	try {
		auto							  surfaceCapabilities	= _physicalDevice->getSurfaceCapabilitiesKHR(*_surface);
		std::vector<vk::SurfaceFormatKHR> availableFormats		= _physicalDevice->getSurfaceFormatsKHR(*_surface);
		std::vector<vk::PresentModeKHR>	  availablePresentModes = _physicalDevice->getSurfacePresentModesKHR(*_surface);

		_swapSurfaceFormat	   = chooseSwapSurfaceFormat(availableFormats);
		_swapPresentMode	   = chooseSwapPresentMode(availablePresentModes);
		_swapExtent			   = chooseSwapExtent(surfaceCapabilities);
		uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo = vk::SwapchainCreateInfoKHR()
																 .setSurface(*_surface)
																 .setMinImageCount(minImageCount)
																 .setImageFormat(_swapSurfaceFormat.format)
																 .setImageColorSpace(_swapSurfaceFormat.colorSpace)
																 .setImageExtent(_swapExtent)
																 .setImageArrayLayers(1)
																 .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
																 .setImageSharingMode(vk::SharingMode::eExclusive)
																 .setPreTransform(surfaceCapabilities.currentTransform)
																 .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
																 .setPresentMode(chooseSwapPresentMode(availablePresentModes))
																 .setClipped(true);

		_swapChain		 = vk::raii::SwapchainKHR(*_device, swapChainCreateInfo);
		_swapChainImages = _swapChain->getImages();
		std::cout << "Swap chain created with " << _swapChainImages.size() << " images" << std::endl;
	} catch (const vk::SystemError& err) {
		throw std::runtime_error(std::string("Vulkan system error: ") + err.what());
	} catch (const std::exception& e) {
		std::cerr << "Failed to create swap chain: " << e.what() << std::endl;
	}
}

void Renderer::createImageViews() {
	assert(_swapChainImageViews.empty());

	vk::ImageViewCreateInfo imageViewCreateInfo = vk::ImageViewCreateInfo()
														  .setViewType(vk::ImageViewType::e2D)
														  .setFormat(_swapSurfaceFormat.format)
														  .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
	imageViewCreateInfo.components				= {
			vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};

	for (auto& image : _swapChainImages) {
		imageViewCreateInfo.image = image;
		_swapChainImageViews.emplace_back(*_device, imageViewCreateInfo);
	}

	std::cout << "Image views created" << std::endl;
}

[[nodiscard]] vk::raii::ShaderModule Renderer::createShaderModule(const std::vector<char>& code) const {
	vk::ShaderModuleCreateInfo createInfo =
			vk::ShaderModuleCreateInfo().setCodeSize(code.size() * sizeof(char)).setPCode(reinterpret_cast<const uint32_t*>(code.data()));
	vk::raii::ShaderModule shaderModule{*_device, createInfo};

	return shaderModule;
}

void Renderer::createGraphicsPipeline() {
	vk::raii::ShaderModule			  shaderModule = createShaderModule(readFile("./shaders/slang.spv"));
	vk::PipelineShaderStageCreateInfo vertShaderCreateInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(shaderModule).setPName("vertMain");
	vk::PipelineShaderStageCreateInfo fragShaderCreateInfo =
			vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(shaderModule).setPName("fragMain");

	vk::PipelineShaderStageCreateInfo		 shaderStages[] = {vertShaderCreateInfo, fragShaderCreateInfo};
	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
			vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizer = vk::PipelineRasterizationStateCreateInfo()
																  .setDepthClampEnable(vk::False)
																  .setRasterizerDiscardEnable(vk::False)
																  .setPolygonMode(vk::PolygonMode::eFill)
																  .setCullMode(vk::CullModeFlagBits::eBack)
																  .setFrontFace(vk::FrontFace::eClockwise)
																  .setDepthBiasEnable(vk::False)
																  .setLineWidth(1.0f);

	vk::PipelineMultisampleStateCreateInfo multisampleInfo =
			vk::PipelineMultisampleStateCreateInfo().setSampleShadingEnable(vk::False).setRasterizationSamples(vk::SampleCountFlagBits::e1);

	vk::PipelineColorBlendAttachmentState colorBlendAttachment = vk::PipelineColorBlendAttachmentState().setBlendEnable(vk::False).setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	vk::PipelineColorBlendStateCreateInfo colorBlending = vk::PipelineColorBlendStateCreateInfo()
																  .setAttachments(colorBlendAttachment)
																  .setLogicOpEnable(vk::False)
																  .setLogicOp(vk::LogicOp::eCopy)
																  .setAttachmentCount(1);

	std::vector<vk::DynamicState>	   dynamicStates		  = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo()
																		.setPDynamicStates(dynamicStates.data())
																		.setDynamicStateCount(static_cast<uint32_t>(dynamicStates.size()));

	vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(_swapExtent.width), static_cast<float>(_swapExtent.height), 0.0f, 1.0f};
	vk::Rect2D	 scissor{vk::Offset2D{0, 0}, _swapExtent};

	vk::PipelineViewportStateCreateInfo viewportState =
			vk::PipelineViewportStateCreateInfo().setViewportCount(1).setPViewports(&viewport).setScissorCount(1).setPScissors(&scissor);

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(0).setPushConstantRangeCount(0);
	_pipelineLayout									= vk::raii::PipelineLayout(*_device, pipelineLayoutInfo);
	vk::PipelineRenderingCreateInfo pipelineRenderingInfo =
			vk::PipelineRenderingCreateInfo().setColorAttachmentCount(1).setPColorAttachmentFormats(&_swapSurfaceFormat.format);

	vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo.stageCount		   = 2;
	pipelineCreateInfo.pStages			   = shaderStages;
	pipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineCreateInfo.pViewportState	   = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState   = &multisampleInfo;
	pipelineCreateInfo.pColorBlendState	   = &colorBlending;
	pipelineCreateInfo.pDynamicState	   = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout			   = _pipelineLayout;
	pipelineCreateInfo.renderPass		   = nullptr;

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain(pipelineCreateInfo,
																												pipelineRenderingInfo);
	_graphicsPipeline = vk::raii::Pipeline(*_device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	std::cout << "Created Graphics pipeline" << std::endl;
}

void Renderer::createCommandPool() {
	vk::CommandPoolCreateInfo poolInfo =
			vk::CommandPoolCreateInfo().setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer).setQueueFamilyIndex(_queueIndex);
	_commandPool = vk::raii::CommandPool(*_device, poolInfo);
}

void Renderer::createCommandBuffer() {
	vk::CommandBufferAllocateInfo allocInfo =
			vk::CommandBufferAllocateInfo().setCommandPool(_commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
	_commandBuffer = std::move(vk::raii::CommandBuffers(*_device, allocInfo).front());
}

void Renderer::transition_image_layout(uint32_t				   imageIndex,
									   vk::ImageLayout		   old_layout,
									   vk::ImageLayout		   new_layout,
									   vk::AccessFlags2		   src_access_mask,
									   vk::AccessFlags2		   dst_access_mask,
									   vk::PipelineStageFlags2 src_stage_mask,
									   vk::PipelineStageFlags2 dst_stage_mask) {
	vk::ImageMemoryBarrier2 barrier = vk::ImageMemoryBarrier2()
											  .setSrcStageMask(src_stage_mask)
											  .setSrcAccessMask(src_access_mask)
											  .setDstStageMask(dst_stage_mask)
											  .setDstAccessMask(dst_access_mask)
											  .setOldLayout(old_layout)
											  .setNewLayout(new_layout)
											  .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
											  .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
											  .setImage(_swapChainImages[imageIndex])
											  .setSubresourceRange(vk::ImageSubresourceRange()
																		   .setAspectMask(vk::ImageAspectFlagBits::eColor)
																		   .setBaseMipLevel(0)
																		   .setLevelCount(1)
																		   .setBaseArrayLayer(0)
																		   .setLayerCount(1));

	vk::DependencyInfo dependency_info = vk::DependencyInfo().setDependencyFlags({}).setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&barrier);
	_commandBuffer.pipelineBarrier2(dependency_info);
}

void Renderer::recordCommandBuffer(uint32_t imageIndex) {
	_commandBuffer.begin({});

	transition_image_layout(imageIndex,
							vk::ImageLayout::eUndefined,
							vk::ImageLayout::eColorAttachmentOptimal,
							{},
							vk::AccessFlagBits2::eColorAttachmentWrite,
							vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							vk::PipelineStageFlagBits2::eColorAttachmentOutput);

	vk::ClearValue				clearColor	   = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::RenderingAttachmentInfo attachmentInfo = vk::RenderingAttachmentInfo()
														 .setImageView(_swapChainImageViews[imageIndex])
														 .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
														 .setLoadOp(vk::AttachmentLoadOp::eClear)
														 .setStoreOp(vk::AttachmentStoreOp::eStore)
														 .setClearValue(clearColor);

	vk::RenderingInfo renderingInfo = vk::RenderingInfo()
											  .setRenderArea(vk::Rect2D().setOffset(vk::Offset2D().setX(0).setY(0)).setExtent(_swapExtent))
											  .setLayerCount(1)
											  .setColorAttachments(attachmentInfo);

	_commandBuffer.beginRendering(renderingInfo);

	_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline);

	_commandBuffer.setViewport(0,
							   vk::Viewport(0.0f, 0.0f, static_cast<float>(_swapExtent.width), static_cast<float>(_swapExtent.height), 0.0f, 1.0f));
	_commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapExtent));

	_commandBuffer.draw(3, 1, 0, 0);

	transition_image_layout(imageIndex,
							vk::ImageLayout::eColorAttachmentOptimal,
							vk::ImageLayout::ePresentSrcKHR,
							vk::AccessFlagBits2::eColorAttachmentWrite,
							{},
							vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							vk::PipelineStageFlagBits2::eBottomOfPipe);

	_commandBuffer.endRendering();
}

void Renderer::drawFrame() {}

void Renderer::destroy() {
	_graphicsQueue.reset();
	_device.reset();
	_physicalDevice.reset();
	_surface.reset();
	_instance.reset();
	SDL_DestroyWindow(_window);
	SDL_Quit();
	std::cout << "Destroyed renderer" << std::endl;
}

void Renderer::run() {
	while (isRunning) {
		if (SDL_PollEvent(_event)) {
			if (_event->type == SDL_EVENT_QUIT) {
				isRunning = false;
			}
			drawFrame();
		}
	}
}
