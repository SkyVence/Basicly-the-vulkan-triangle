#pragma once

namespace Engine {
	class Window {
	  public:
		Window() = default;
		virtual ~Window() = default;

		// Prevent copying
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		// Pure virtual methods for derived classes to implement
		virtual void create_window() = 0;
		virtual void destroy_window() = 0;
		virtual void handle_input() = 0;
	};
} // namespace Engine
