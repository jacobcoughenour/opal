#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "../typedefs.h"
#include "config.h"
#include "shader.h"

// #define VMA_IMPLEMENTATION
// #define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#include <volk.h>

#include <VkBootstrap.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <array>
#include <cstring>
#include <optional>
#include <set>
#include <vector>

namespace Opal {

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 2>
	get_attribute_descriptions() {
		std::array<VkVertexInputAttributeDescription, 2>
				attribute_descriptions{};

		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, color);

		return attribute_descriptions;
	}
};

const std::vector<Vertex> vertices = { { { 0.0f, -0.5f },
											   { 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
	{ { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } } };

class Renderer {

public:
	Error initialize();
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

	// vk mem allocator
	VmaAllocator _vma_allocator;

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

	// buffers
	VkBuffer _vertex_buffer;
	VmaAllocation _vertex_buffer_alloc;

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

	Error create_window();
	Error create_vk_instance();
	Error create_surface();
	Error create_vk_device();
	Error create_vma_allocator();
	Error create_swapchain();
	Error get_queues();
	Error create_render_pass();
	Error create_graphics_pipeline();
	Error create_framebuffers();
	Error create_command_pool();
	Error create_vertex_buffer();
	Error create_command_buffers();
	Error create_sync_objects();

	Error recreate_swapchain();

	Error draw_frame();
};

} // namespace Opal

#endif // __RENDERER_H__