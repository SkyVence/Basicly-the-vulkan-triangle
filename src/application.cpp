#include "application.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <cassert>

Application::Application()	= default;
Application::~Application() = default;

void Application::createWindow(int width, int height) {
	SDL_Init(SDL_INIT_VIDEO);
	app_window = SDL_CreateWindow("VK Learn", width, height, 0);
}

void Application::run() {
	app_running = true;

	while (app_running) {
		while (SDL_PollEvent(&app_event)) {
			if (app_event.type == SDL_EVENT_QUIT) {
				app_running = false;
			}
		}
	}

	SDL_DestroyWindow(app_window);
	SDL_Quit();
}
