#include "window/sdl3_window.hpp"

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "window/window.hpp"

#include <exception>
#include <stdexcept>
#include <string>

using namespace Engine;

SDL3_Window::SDL3_Window(std::string title, int width, int height, SDL_WindowFlags flags)
	: Window(), w_title(title), w_width(width), w_height(height), _window(nullptr), _event(), _window_flags(flags) {
	create_window();
}

SDL3_Window::~SDL3_Window() { destroy_window(); }

void SDL3_Window::create_window() {
	try {
		SDL_Init(SDL_INIT_VIDEO);

		_window = SDL_CreateWindow(w_title.c_str(), w_width, w_height, _window_flags);
		if (!_window) {
			throw std::runtime_error("Failed to create SDL window");
		}
	} catch (const std::exception& e) {
		throw std::runtime_error("SDL_Init failed: " + std::string(e.what()));
		SDL_Quit();
	}
}

void SDL3_Window::destroy_window() {
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = nullptr;
		SDL_Quit();
	}
}

void SDL3_Window::handle_input() {
	// Handle SDL3 input events here
	// This will be called from the main engine loop
}

SDL_Window* SDL3_Window::get_window() const { return _window; }

SDL_Event SDL3_Window::get_event() { return _event; }

SDL_Event* SDL3_Window::get_event_ptr() { return &_event; }
