#include "SDL3/SDL_video.h"
#include "renderer.hpp"
#include "window/sdl3_window.hpp"

#include <string>

namespace Engine {
	class Application {
	  public:
		Application();
		~Application();
		void start(std::string title, int width, int height, SDL_WindowFlags flags);
		void run();

	  private:
		Renderer*	 _renderer;
		SDL3_Window* _window;
	};
} // namespace Engine
