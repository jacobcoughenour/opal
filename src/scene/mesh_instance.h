#ifndef __MESH_INSTANCE_H__
#define __MESH_INSTANCE_H__

#include "scene.h"

namespace Opal {

class MeshInstance : public Node3D {

protected:
	Renderer::Mesh *_mesh;
	Material *_material;

public:
	MeshInstance() : Node3D("MeshInstance") {}
	MeshInstance(const char *name) : Node3D(name) {}
	MeshInstance(const char *name, Renderer::Mesh *mesh) : MeshInstance(name) {
		set_mesh(mesh);
	}

	void set_mesh(Renderer::Mesh *mesh);
	void set_material(Material *material);
	void init();
	void update(float delta);
	void draw(DrawContext *context);
};

} // namespace Opal
#endif // __MESH_INSTANCE_H__