#include "SDL3/SDL_video.h"
#include "engine.hpp"

#include <exception>
#include <iostream>

int main() {
	try {
		Engine::Application app;
		app.start("Vulkan Renderer", 1280, 720, SDL_WINDOW_VULKAN);
		app.run();
	} catch (const std::exception& e) {
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
}
