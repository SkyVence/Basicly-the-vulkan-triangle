#pragma once

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "window.hpp"

#include <cstdint>
#include <string>

namespace Engine {

	class SDL3_Window : public Window {
	  public:
		SDL3_Window(std::string title, int width, int height, SDL_WindowFlags flags);
		~SDL3_Window();

		// Window propertiess
		std::string w_title;
		uint32_t	w_width;
		uint32_t	w_height;
		SDL_Event	get_event();
		SDL_Window* get_window() const;
		SDL_Event*	get_event_ptr();

	  private:
		SDL_Window*		_window;
		SDL_Event		_event;
		SDL_WindowFlags _window_flags;

		void create_window();
		void destroy_window();
		void handle_input();
	};
} // namespace Engine
