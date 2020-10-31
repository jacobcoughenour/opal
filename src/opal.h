#ifndef OPAL_OPAL_H
#define OPAL_OPAL_H

#include <vulkan/vulkan.hpp>

#define NVVK_ALLOC_DEDICATED
#include <nvvk/allocator_vk.hpp>
#include <nvvk/appbase_vkpp.hpp>
#include <nvvk/debug_util_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>

#include <nvvk/raytraceKHR_vk.hpp>

class Opal : public nvvk::AppBase {
public:
	struct ObjModel {
		uint32_t indices{ 0 };
		uint32_t vertices{ 0 };
		nvvk::Buffer vertex_buffer;
		nvvk::Buffer index_buffer;
		nvvk::Buffer mat_color_buffer;
		nvvk::Buffer mat_index_buffer;
	};

	struct ObjInstance {
		uint32_t object_index{ 0 };
		uint32_t texture_offset{ 0 };
		nvmath::mat4f transform{ 1 };
		nvmath::mat4f transform_inverse{ 1 };
	};

	struct ObjPushConstant {
		nvmath::vec3f light_position{ 10.f, 15.f, 8.f };
		int instance_id{ 0 };
		float light_intensity{ 100.f };
		int light_type{ 0 };
	};

	struct CameraMatrices {
		nvmath::mat4f view;
		nvmath::mat4f proj;
		nvmath::mat4f view_inverse;
		nvmath::mat4f proj_inverse;
	};

	void setup(const vk::Instance &instance,
			const vk::Device &device,
			const vk::PhysicalDevice &physicalDevice,
			uint32_t graphicsQueueIndex,
			const vk::SurfaceKHR &surface,
			uint32_t width,
			uint32_t height);
	void destroyResources();
	void render();
	void loadModel(const std::string &filename, nvmath::mat4f transform = nvmath::mat4f(1));
	void onResize(int, int) override;

private:
	void updateUniformBuffer();
	void createTextureImages(const vk::CommandBuffer &cmd_buf, const std::vector<std::string> &p_textures);

	bool raytracing_enabled = false;

	nvmath::vec4f clear_color = nvmath::vec4f(1, 1, 1, 1.f);
	vk::ClearValue clear_color_values[2];

	ObjPushConstant push_constant;

	// scene objects

	std::vector<ObjModel> object_models;
	std::vector<ObjInstance> object_instances;

	nvvk::Buffer camera_mat;
	nvvk::Buffer scene_descriptor;
	std::vector<nvvk::Texture> textures;

	nvvk::AllocatorDedicated alloc;
	nvvk::DebugUtil debug;

	// main pipeline

	vk::Pipeline graphics_pipeline;
	vk::PipelineLayout pipeline_layout;
	vk::DescriptorPool descriptor_pool;
	vk::DescriptorSet descriptor_set;
	vk::DescriptorSetLayout descriptor_set_layout;
	nvvk::DescriptorSetBindings descriptor_set_layout_bindings;

	void createGraphicsPipeline();
	void createDescriptorSetLayout();
	void createUniformBuffer();
	void createSceneDescriptionBuffer();
	void updateDescriptorSet();

	// post pipeline

	vk::Pipeline post_graphics_pipeline;
	vk::PipelineLayout post_pipeline_layout;
	vk::DescriptorPool post_descriptor_pool;
	vk::DescriptorSet post_descriptor_set;
	vk::DescriptorSetLayout post_descriptor_set_layout;
	nvvk::DescriptorSetBindings post_descriptor_set_layout_bindings;

	vk::RenderPass offscreen_render_pass;
	vk::Framebuffer offscreen_framebuffer;
	nvvk::Texture offscreen_color_texture;
	vk::Format offscreen_color_format{ vk::Format::eR32G32B32A32Sfloat };
	nvvk::Texture offscreen_depth_texture;
	vk::Format offscreen_depth_format{ vk::Format::eD32Sfloat };

	/**
	 * create offscreen framebuffer and render pass
	 */
	void createOffscreenRender();
	void createPostPipeline();
	void createPostDescriptor();
	void updatePostDescriptorSet();
	void drawPost(const vk::CommandBuffer &cmd_buf);

	// rendering methods

	void resetFrame();
	void updateFrame();

	/**
	 * rasterizes the scene to the command buffer
	 * @param cmd_buf
	 */
	void rasterize(const vk::CommandBuffer &cmd_buf);

	/**
	 * ray-traces the scene to the command buffer
	 * @param cmd_buf
	 */
	void raytrace(const vk::CommandBuffer &cmd_buf);

	// Ray-tracing

	vk::PhysicalDeviceRayTracingPropertiesKHR rt_properties;
	nvvk::RaytracingBuilderKHR rt_builder;
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> rt_shader_groups;
	nvvk::Buffer rt_sbt_buffer;

	vk::Pipeline rt_graphics_pipeline;
	vk::PipelineLayout rt_pipeline_layout;
	vk::DescriptorPool rt_descriptor_pool;
	vk::DescriptorSet rt_descriptor_set;
	vk::DescriptorSetLayout rt_descriptor_set_layout;
	nvvk::DescriptorSetBindings rt_descriptor_set_layout_bindings;

	struct RtPushConstant {
		nvmath::vec4f clear_color;
		nvmath::vec3f light_position;
		float light_intensity;
		int light_type;
		int frame{ 0 };
	} rt_push_constants;

	/**
	 * initialize ray tracing features
	 */
	void initRayTracing();

	/**
	 * converts object geometry to ray tracing acceleration structure
	 */
	nvvk::RaytracingBuilderKHR::Blas objectToVkGeometryKHR(const ObjModel &model);

	/**
	 * create bottom level acceleration structure
	 */
	void createBottomLevelAS();

	/**
	 * create top level acceleration structure
	 */
	void createTopLevelAS();

	/**
	 * create descriptor set for raytracing pipeline
	 */
	void createRtDescriptorSet();

	/**
	 * update existing descriptor set for raytracing pipeline
	 */
	void updateRtDescriptorSet();

	/**
	 * create raytracing graphics pipeline
	 */
	void createRtPipeline();

	/**
	 * creates the shader binding table (SBT)
	 */
	void createRtShaderBindingTable();
};

#endif // OPAL_OPAL_H
