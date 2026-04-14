#include "application.hpp"
#include <cstdlib>
#include <cstdint>

struct Window {
  uint32_t width;
  uint32_t height;
};

int main(int argc, char* argv[]) {
  Window window;
  window.width = argc > 1 ? atoi(argv[1]) : 800;
  window.height = argc > 2 ? atoi(argv[2]) : 600;
  Application app;
  app.createWindow(window.width, window.height);
  app.run();
}
