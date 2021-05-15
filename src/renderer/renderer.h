#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "../utils/log.h"

#include <volk.h>

#include <VkBootstrap.h>

#include <GLFW/glfw3.h>

namespace Opal {

class Renderer {

public:
	int initialize();
	void destroy();
	void start_render_loop();

protected:
	vkb::Instance _vkb_instance;
	vkb::Device _device;
	VkSurfaceKHR _vk_surface;
	GLFWwindow *_window;

	void render();
};

} // namespace Opal

#endif // __RENDERER_H__