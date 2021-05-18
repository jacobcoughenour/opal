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

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <optional>
#include <set>
#include <vector>

using namespace std;
using namespace glm;

namespace Opal {

struct Vertex {
	vec2 pos;
	vec3 color;
	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static array<VkVertexInputAttributeDescription, 2>
	get_attribute_descriptions() {
		array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

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

struct UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
};

const vector<Vertex> vertices = { { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } } };

const vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

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

	// vk mem allocator
	VmaAllocator _vma_allocator;

	// swapchain
	vkb::Swapchain _vkb_swapchain;

	// queues
	VkQueue _graphics_queue;
	VkQueue _present_queue;

	VkDescriptorSetLayout _descriptor_set_layout;
	VkDescriptorPool _descriptor_pool;
	vector<VkDescriptorSet> _descriptor_sets;

	// graphics pipeline
	VkPipelineLayout _pipeline_layout;
	VkPipeline _graphics_pipeline;
	VkRenderPass _render_pass;

	// commands
	VkCommandPool _command_pool;
	vector<VkCommandBuffer> _command_buffers;

	// images
	vector<VkImage> _swapchain_images;
	vector<VkImageView> _swapchain_image_views;
	vector<VkFramebuffer> _framebuffers;

	// semaphores
	vector<VkSemaphore> _available_semaphores;
	vector<VkSemaphore> _finished_semaphores;
	vector<VkFence> _in_flight_fences;
	vector<VkFence> _images_in_flight;

	size_t _current_frame = 0;

	Error create_window();
	Error create_vk_instance();
	Error create_surface();
	Error create_vk_device();
	Error create_vma_allocator();
	Error create_swapchain();
	Error create_image_views();
	Error get_queues();
	Error create_render_pass();
	Error create_descriptor_set_layout();
	Error create_graphics_pipeline();
	Error create_framebuffers();
	Error create_command_pool();
	Error create_vertex_buffer();
	Error create_index_buffer();
	Error create_uniform_buffers();
	Error create_descriptor_pool();
	Error create_descriptor_sets();
	Error create_command_buffers();
	Error create_sync_objects();

	Error recreate_swapchain();
	Error destroy_swapchain();

	Error draw_frame();

	// buffers

	struct Buffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation alloc = nullptr;
		uint32_t size = 0;
		uint32_t usage = 0;
		VkDescriptorBufferInfo info;
		Buffer() {}
	};

	Buffer _vertex_buffer;
	Buffer _index_buffer;
	vector<Buffer> _uniform_buffers;

	Error create_buffer(Buffer *buffer,
			uint32_t size,
			uint32_t usage,
			VmaMemoryUsage mapping,
			VkMemoryPropertyFlags mem_flags);
	Error copy_buffer(Buffer *src_buffer, Buffer *dst_buffer, uint32_t size);
	Error destroy_and_free_buffer(Buffer *buffer);

	Error update_uniform_buffer(uint32_t image_index);
};

} // namespace Opal

#endif // __RENDERER_H__