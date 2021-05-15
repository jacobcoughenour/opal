#include "app.h"

using namespace Opal;

App::App(int argc, char **argv) {
	_renderer = Renderer{};
}

int App::run() {

	if (_renderer.initialize() != 0) {
		return EXIT_FAILURE;
	}

	_renderer.start_render_loop();

	_renderer.destroy();

	return EXIT_SUCCESS;
}
