#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "../typedefs.h"
#include "vk_types.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "vk_shader.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Opal {

const std::string TEXTURE_PATH = "assets/models/viking_room.png";

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription desc {
			.binding   = 0,
			.stride	   = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		return desc;
	}

	static std::array<VkVertexInputAttributeDescription, 3>
	get_attribute_descriptions() {
		std::array<VkVertexInputAttributeDescription, 3>
				attribute_descriptions {};

		attribute_descriptions[0].binding  = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[0].offset   = offsetof(Vertex, pos);

		attribute_descriptions[1].binding  = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset   = offsetof(Vertex, color);

		attribute_descriptions[2].binding  = 0;
		attribute_descriptions[2].location = 2;
		attribute_descriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[2].offset   = offsetof(Vertex, tex_coord);

		return attribute_descriptions;
	}

	bool operator==(const Vertex &other) const {
		return pos == other.pos && color == other.color &&
			   tex_coord == other.tex_coord;
	}
};

class RenderObject {
public:
	struct DrawContext {
		VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
		uint32_t image_index;
		RenderObject *prev_object = nullptr;
	};

	RenderObject() : name("RenderObject") {}
	explicit RenderObject(const char *name) : name(name) {}

	glm::mat4 transform;
	const char *name = nullptr;

	virtual void init()					   = 0;
	virtual void draw(DrawContext context) = 0;

	virtual ~RenderObject() = default;
};

class Renderer {

protected:
	static Renderer *singleton;

public:
	Error initialize();
	void destroy();
	void start_render_loop();

	static Renderer *get_singleton();

	struct UniformBufferObject {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};

	struct Image {
		VkImage image		= VK_NULL_HANDLE;
		VmaAllocation alloc = nullptr;
		VkExtent3D extent;
		VkFormat format			= VK_FORMAT_UNDEFINED;
		VkImageTiling tiling	= VK_IMAGE_TILING_MAX_ENUM;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
		VkMemoryPropertyFlags properties =
				VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
	};

	struct Buffer {
		const char *name	= nullptr;
		VkBuffer buffer		= VK_NULL_HANDLE;
		VmaAllocation alloc = nullptr;
		uint32_t size		= 0;
		uint32_t usage		= 0;
		VkDescriptorBufferInfo info;
		Buffer() {}
	};

	// todo manage descriptors

	struct Uniform {};

	struct Shader {
		std::vector<Uniform> uniforms;
	};

	struct Mesh {
		const char *name = nullptr;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		Buffer vertex_buffer;
		Buffer index_buffer;

		static Error load_from_obj(Mesh *mesh, const char *filename);
	};

	bool has_mesh(Mesh *mesh);
	void add_mesh(Mesh *mesh);
	void add_render_object(RenderObject *render_object);

protected:
	bool _initialized;

	std::set<Mesh *> _meshes;

	std::vector<RenderObject *> _render_objects;

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

public:
	std::vector<VkDescriptorSet> _descriptor_sets;

	// graphics pipeline
	VkPipelineLayout _pipeline_layout;
	VkPipeline _graphics_pipeline;

protected:
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
	std::vector<VkFence> _images_in_flight;

	size_t _current_frame = 0;

	Error create_window();
	Error create_vk_instance();
	Error create_surface();
	Error create_vk_device();
	Error create_vma_allocator();
	Error create_swapchain();
	// Error create_image_views();
	Error get_queues();
	Error create_render_pass();
	Error create_descriptor_set_layout();
	Error create_graphics_pipeline();
	Error create_framebuffers();
	Error create_command_pool();
	Error create_depth_resources();
	Error create_texture_image();
	Error create_texture_image_view();
	Error create_texture_sampler();

	Error create_uniform_buffers();
	Error create_descriptor_pool();
	Error create_descriptor_sets();
	Error create_command_buffers();
	Error create_sync_objects();

	VkFormat find_supported_format(
			const std::vector<VkFormat> &candidates,
			VkImageTiling tiling,
			VkFormatFeatureFlags features);
	VkFormat find_depth_format();
	bool has_stencil_component(VkFormat format);

	Error draw_scene(VkCommandBuffer cmd_buf, uint32_t image_index);
	Error draw_frame();

	Error recreate_swapchain();
	Error destroy_swapchain();

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
	ERR_FAIL_COND_V_MSG(                                                       \
			!cmd_buf,                                                          \
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
	void
	_end_and_submit_single_use_command_buffer(VkCommandBuffer command_buffer);

	// images

	Image _texture_image;
	VkSampler _texture_sampler;
	VkImageView _texture_image_view;

	Image _depth_image;
	VkImageView _depth_image_view;

	Error create_image(
			Image *image,
			uint32_t width,
			uint32_t height,
			VkFormat format,
			VkImageTiling tiling,
			VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties);

	VkImageView create_image_view(
			std::string name,
			VkImage image,
			VkFormat format,
			VkImageAspectFlags aspect);

	Error transition_image_layout(
			Image *image, VkImageLayout old_layout, VkImageLayout new_layout);

	Error destroy_and_free_image(Image *image);

	// buffers

	std::vector<Buffer> _uniform_buffers;

	Error create_buffer(
			Buffer *buffer,
			std::string name,
			uint32_t size,
			uint32_t usage,
			VmaMemoryUsage mapping,
			VkMemoryPropertyFlags mem_flags);
	Error copy_buffer(Buffer *src_buffer, Buffer *dst_buffer, uint32_t size);

	/**
	 * @brief Deallocates and nullifies the given buffer.
	 */
	Error destroy_and_free_buffer(Buffer *buffer);

	/**
	 * @brief wraps vkCmdCopyBufferToImage and submits it.
	 */
	Error copy_buffer_to_image(Buffer *buffer, Image *image);

	Error update_uniform_buffer(uint32_t image_index);

	Renderer::Buffer
	create_vertex_buffer(std::string name, std::vector<Vertex> vertices);
	Renderer::Buffer
	create_index_buffer(std::string name, std::vector<uint32_t> indices);
};

} // namespace Opal

namespace std {
template <> struct hash<Opal::Vertex> {
	size_t operator()(Opal::Vertex const &vertex) const {
		return ((hash<glm::vec3>()(vertex.pos) ^
				 (hash<glm::vec3>()(vertex.color) << 1)) >>
				1) ^
			   (hash<glm::vec2>()(vertex.tex_coord) << 1);
	}
};
} // namespace std

#endif // __RENDERER_H__