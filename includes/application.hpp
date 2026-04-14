#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <cstddef>

class Application {
public:
    Application();
    ~Application();

    void createWindow(int width, int height);
    void run();
private:
    SDL_Window* app_window = NULL;
    bool app_running = false;
    SDL_Event app_event;
};
