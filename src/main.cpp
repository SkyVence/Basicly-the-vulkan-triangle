#include "application.hpp"

int main() {
	App app;
	app.getRenderer().create();
	app.getRenderer().run();
}
