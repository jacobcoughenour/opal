#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "../utils/log.h"
#include "config.h"
#include "shader.h"

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
	bool _initialized;

	// window stuff
	GLFWwindow *_window;
	VkSurfaceKHR _vk_surface;

	// vulkan instances
	vkb::Instance _vkb_instance;
	vkb::Device _vkb_device;
	vkb::Swapchain _vkb_swapchain;

	// queues
	VkQueue _graphics_queue;
	VkQueue _present_queue;

	// graphics pipeline
	VkPipelineLayout _pipeline_layout;
	VkPipeline _graphics_pipeline;
	VkRenderPass _render_pass;

	// commands
	VkCommandPool _command_pool;
	std::vector<VkCommandBuffer> _command_buffers;

	// images
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	std::vector<VkFramebuffer> _framebuffers;

	// semaphores
	std::vector<VkSemaphore> _available_semaphores;
	std::vector<VkSemaphore> _finished_semaphores;
	std::vector<VkFence> _in_flight_fences;
	std::vector<VkFence> _image_in_flight;

	size_t _current_frame = 0;

	int create_window();
	int create_vk_instance();
	int create_surface();
	int create_vk_device();
	int create_swapchain();
	int get_queues();
	int create_render_pass();
	int create_graphics_pipeline();
	int create_framebuffers();
	int create_command_pool();
	int create_command_buffers();
	int create_sync_objects();

	int recreate_swapchain();

	int draw_frame();
};

} // namespace Opal

#endif // __RENDERER_H__