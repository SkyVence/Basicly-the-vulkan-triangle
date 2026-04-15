#include "engine.hpp"

#include "SDL3/SDL_video.h"
#include "renderer.hpp"
#include "window/sdl3_window.hpp"

#include <string>

using namespace Engine;

Application::Application() {
	_window	  = nullptr;
	_renderer = nullptr;
}

Application::~Application() {}

void Application::start(std::string title, int width, int height, SDL_WindowFlags flags) {
	_window	  = new SDL3_Window(title, width, height, flags);
	_renderer = new Renderer(_window->get_window(), _window->get_event_ptr());
}

void Application::run() { _renderer->run(); }
