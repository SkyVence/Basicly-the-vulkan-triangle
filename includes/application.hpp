#include "renderer.hpp"

class App {
  public:
	App();
	~App();
	Engine::Renderer& getRenderer();

  private:
	Engine::Renderer _renderer;
};
