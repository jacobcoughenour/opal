#ifndef __APP_H__
#define __APP_H__

#include "renderer/renderer.h"
#include "scene/mesh_instance.h"
#include "scene/scene.h"
#include "utils/log.h"
#include <cstdlib>

namespace Opal {

class DemoNode : public Node3D {
public:
	DemoNode() : Node3D("DemoNode") {}

	void input_key(int key, int scancode, int action, int mods) override;
};

class App {

public:
	App(int argc, char **argv);
	int run();

protected:
	Renderer _renderer;
};

} // namespace Opal

#endif // __APP_H__