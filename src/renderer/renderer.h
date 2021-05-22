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

	Error draw_frame();

	Error recreate_swapchain();
	Error destroy_swapchain();

#ifdef USE_DEBUG_UTILS
#define VK_DEBUG_OBJECT_NAME(type, handle, name)                               \
	_debug_object_name(type, handle, name)
	void _debug_object_name(
			VkObjectType type, uint64_t handle, const char *name);
#define VK_DEBUG_BEGIN_LABEL(command_buffer, name, r, g, b, a)                 \
	_debug_begin_label(command_buffer, name, r, g, b, a)
	void _debug_begin_label(VkCommandBuffer command_buffer,
			const char *name,
			float r,
			float g,
			float b,
			float a);
#define VK_DEBUG_INSERT_LABEL(command_buffer, name, r, g, b, a)                \
	_debug_insert_label(command_buffer, name, r, g, b, a)
	void _debug_insert_label(VkCommandBuffer command_buffer,
			const char *name,
			float r,
			float g,
			float b,
			float a);
#define VK_DEBUG_END_LABEL(command_buffer) _debug_end_label(command_buffer)
	void _debug_end_label(VkCommandBuffer command_buffer);
#else
#define VK_DEBUG_OBJECT_NAME(type, handle, name) ((void)0)
#define VK_DEBUG_BEGIN_LABEL(command_buffer, name, r, g, b, a) ((void)0)
#define VK_DEBUG_INSERT_LABEL(command_buffer, name, r, g, b, a) ((void)0)
#define VK_DEBUG_END_LABEL(command_buffer) ((void)0)
#endif

/**
 * @brief Utility macro for running a single time command.
 *
 * This creates a single use command buffer, adds your command to it, then
 * submits the command to Vulkan. This works with all the `vkCmd` prefixed
 * Vulkan functions since their first param is VkCommandBuffer. So the
 * `__VA_ARGS__` match the original function's parameters without the first
 * command buffer parameter.
 */
#define VK_SUBMIT_SINGLE_CMD_OR_FAIL(vkCmd, ...)                               \
	/* create single-use command buffer */                                     \
	VkCommandBuffer cmd_buf = _begin_single_use_command_buffer();              \
	/* null check the command buffer */                                        \
	ERR_FAIL_COND_V_MSG(!cmd_buf,                                              \
			FAIL,                                                              \
			"Failed to create command buffer for single time command");        \
	/* insert command in command buffer */                                     \
	vkCmd(cmd_buf, __VA_ARGS__);                                               \
	/* end buffer and submit command */                                        \
	_end_and_submit_single_use_command_buffer(cmd_buf);                        \
	((void)0)

	/**
	 * Allocates a "single-use" command buffer for running a command.
	 * Use the `VK_SUBMIT_SINGLE_CMD` macro instead.
	 */
	VkCommandBuffer _begin_single_use_command_buffer();

	/**
	 * Ends and deallocates a "single-use" command buffer.
	 * Use the `VK_SUBMIT_SINGLE_CMD` macro instead.
	 */
	void _end_and_submit_single_use_command_buffer(
			VkCommandBuffer command_buffer);

	struct Image {
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation alloc = nullptr;
		VkExtent3D extent;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageTiling tiling = VK_IMAGE_TILING_MAX_ENUM;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
		VkMemoryPropertyFlags properties =
				VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
	};

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