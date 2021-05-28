#ifndef __RENDER_OBJECT_H__
#define __RENDER_OBJECT_H__

#include "renderer.h"
#include "vk_types.h"

namespace Opal {

class MeshInstance : public RenderObject {

protected:
	Renderer::Mesh *_mesh;
	Material *_material;

public:
	MeshInstance() : RenderObject("MeshInstance") {}
	MeshInstance(const char *name) : RenderObject(name) {}
	MeshInstance(const char *name, Renderer::Mesh *mesh) : MeshInstance(name) {
		set_mesh(mesh);
	}

	void set_mesh(Renderer::Mesh *mesh);
	void set_material(Material *material);
	void init();
	void update();
	void draw(DrawContext context);
};

class VolumeInstance : public RenderObject {

	// Volume *volume;

	void init();
	void draw(DrawContext context);
};

} // namespace Opal
#endif // __RENDER_OBJECT_H__