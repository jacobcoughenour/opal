#include "app.h"
#include "renderer/render_object.h"

using namespace Opal;

App::App(int argc, char **argv) {
	_renderer = Renderer {};
}

int App::run() {

	if (_renderer.initialize() != 0) {
		return EXIT_FAILURE;
	}

	Renderer::Mesh mesh_1 { .name = "viking room mesh" };
	Renderer::Mesh::load_from_obj(&mesh_1, "assets/models/viking_room.obj");
	MeshInstance inst_1 { "instance 1", &mesh_1 };
	// _renderer.add_render_object(&inst_1);

	Renderer::Mesh mesh_2 { .name = "sphere mesh" };
	Renderer::Mesh::load_from_obj(&mesh_2, "assets/models/sphere.obj");
	MeshInstance inst_2 { "instance 2", &mesh_2 };
	_renderer.add_render_object(&inst_2);

	Renderer::Mesh mesh_3 { .name = "plane mesh" };
	Renderer::Mesh::load_from_obj(&mesh_3, "assets/models/plane.obj");
	MeshInstance inst_3 { "instance 3", &mesh_3 };
	_renderer.add_render_object(&inst_3);

	_renderer.start_render_loop();

	_renderer.destroy();

	return EXIT_SUCCESS;
}
