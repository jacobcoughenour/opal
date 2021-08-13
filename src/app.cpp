#include "app.h"
#include "utils/log.h"

using namespace Opal;
using namespace glm;

void DemoNode::input_key(int key, int scancode, int action, int mods) {}

App::App(int argc, char **argv) {
	_renderer = Renderer {};
}

int App::run() {

	if (_renderer.initialize() != OK) {
		return EXIT_FAILURE;
	}

	// initialize resources
	//
	// ShaderMaterial material = {
	// 	.fragment_shader = loadShader("shaders/frag_shader.frag"),
	// 	.vertex_shader = loadShader("shaders/vert_shader.vert"),
	// 	.uniforms = {
	// 		texture = loadTexture("textures/.."),
	// 	},
	// };

	// Mesh mesh = {
	// 	.name = "mesh",
	// 	.geometry = CubeGeometry { 1, 1, 1 },
	// 	.material = material,
	// };

	// build the scene
	//
	// Scene scene;

	// MeshInstance mesh_inst = {
	// 	.name = "mesh instance",
	// 	.mesh = mesh,
	// 	.transform = mat4(1.0f),
	// };

	// scene.add(mesh_inst);

	// Camera camera = {
	// 	.fov = 70,
	// 	.near = 0.1,
	// 	.far = 1000,
	// 	.transform = translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.0f)),
	// };

	// renderer.start_render_loop(() => {
	// 	renderer.render(scene, camera);
	// });
	//
	// renderer.destroy();
	//
	// return EXIT_SUCCESS;

	// todo

	Node3D scene { "demo scene" };

	Renderer::Mesh mesh_1 { .name = "viking room mesh" };
	Renderer::Mesh::load_from_obj(&mesh_1, "assets/models/viking_room.obj");
	MeshInstance inst_1 { "instance 1", &mesh_1 };
	inst_1.transform = translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.0f));
	scene.add_child(&inst_1);

	Renderer::Mesh mesh_2 { .name = "sphere mesh" };
	Renderer::Mesh::load_from_obj(&mesh_2, "assets/models/sphere.obj");
	MeshInstance inst_2 { "instance 2", &mesh_2 };
	inst_2.transform =
			scale(translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.5f)), vec3(0.2f));
	inst_1.add_child(&inst_2);

	Renderer::Mesh mesh_3 { .name = "plane mesh" };
	Renderer::Mesh::load_from_obj(&mesh_3, "assets/models/plane.obj");
	MeshInstance inst_3 { "instance 3", &mesh_3 };
	inst_3.transform = rotate(
			translate(scale(mat4(1.0f), vec3(0.05f)), vec3(0.0f, 0.0f, -0.1f)),
			pi<float>() * 0.5f,
			vec3(1.0f, 0.0f, 0.0f));
	scene.add_child(&inst_3);

	DemoNode node {};
	scene.add_child(&node);

	_renderer.set_render_object(&scene);

	scene.print_tree();

	scene.remove_child(&inst_2);

	_renderer.start_render_loop();

	_renderer.destroy();

	return EXIT_SUCCESS;
}
