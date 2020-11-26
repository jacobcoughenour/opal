#include "opal.h"
#include <vulkan/vulkan.hpp>

#include <nvh/cameramanipulator.hpp>
#include <nvh/fileoperations.hpp>

#include <imgui/imgui_impl_glfw.h>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>
#include <nvvk/pipeline_vk.hpp>
#include <nvvk/renderpasses_vk.hpp>
#include <nvvk/shaders_vk.hpp>

#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "fileformats/stb_image.h"
#include "obj_loader.h"

extern std::vector<std::string> default_search_paths;

void Opal::setup(const vk::Instance &instance,
		const vk::Device &device,
		const vk::PhysicalDevice &physicalDevice,
		uint32_t graphicsQueueIndex,
		const vk::SurfaceKHR &surface,
		uint32_t width,
		uint32_t height) {
	AppBase::setup(instance, device, physicalDevice, graphicsQueueIndex);

	alloc.init(device, physicalDevice);
	debug.setup(device);

	createSurface(surface, width, height);

	createDepthBuffer();
	createRenderPass();
	createFrameBuffers();

	initGUI(0);

	// loadModel(nvh::findFile("media/scenes/wuson.obj", default_search_paths));
	// loadModel(nvh::findFile("media/scenes/sphere.obj", default_search_paths),
	// 		nvmath::scale_mat4(nvmath::vec3f(1.5f)) * nvmath::translation_mat4(nvmath::vec3f(0.0f, 1.0f, 0.0f)));
	loadModel(nvh::findFile("media/scenes/plane.obj", default_search_paths));

	// createSpheres();
	createVolumes();

	createOffscreenRender();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createUniformBuffer();
	createSceneDescriptionBuffer();
	updateDescriptorSet();

	// raytracing pipeline
	initRayTracing();
	createBottomLevelAS();
	createTopLevelAS();
	createRtDescriptorSet();
	createRtPipeline();
	createRtShaderBindingTable();

	createPostDescriptor();
	createPostPipeline();
	updatePostDescriptorSet();
}

void Opal::destroyResources() {
	// destroy graphics pipeline
	m_device.destroy(graphics_pipeline);
	m_device.destroy(pipeline_layout);
	m_device.destroy(descriptor_pool);
	m_device.destroy(descriptor_set_layout);

	// destroy scene
	alloc.destroy(camera_mat);
	alloc.destroy(scene_descriptor);

	for (auto &m : object_models) {
		alloc.destroy(m.vertex_buffer);
		alloc.destroy(m.index_buffer);
		alloc.destroy(m.mat_color_buffer);
		alloc.destroy(m.mat_index_buffer);
	}
	for (auto &tex : textures) {
		alloc.destroy(tex);
	}

	// alloc.destroy(spheres_buffer);
	// alloc.destroy(spheres_aabb_buffer);
	// alloc.destroy(spheres_mat_color_buffer);
	// alloc.destroy(spheres_mat_index_buffer);

	alloc.destroy(volumes_buffer);
	alloc.destroy(volumes_aabb_buffer);

	for (auto &tex : volume_density_textures) {
		alloc.destroy(tex);
	}

	// destroy post pipeline

	m_device.destroy(post_graphics_pipeline);
	m_device.destroy(post_pipeline_layout);
	m_device.destroy(post_descriptor_pool);
	m_device.destroy(post_descriptor_set_layout);

	alloc.destroy(offscreen_color_texture);
	alloc.destroy(offscreen_depth_texture);

	m_device.destroy(offscreen_render_pass);
	m_device.destroy(offscreen_framebuffer);

	// destroy raytracing pipeline

	rt_builder.destroy();
	m_device.destroy(rt_descriptor_pool);
	m_device.destroy(rt_descriptor_set_layout);
	m_device.destroy(rt_graphics_pipeline);
	m_device.destroy(rt_pipeline_layout);
	alloc.destroy(rt_sbt_buffer);
}

void Opal::createTextureImages(const vk::CommandBuffer &cmd_buf, const std::vector<std::string> &p_textures) {
	using vkIU = vk::ImageUsageFlagBits;

	vk::SamplerCreateInfo sampler_info{ {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear };
	sampler_info.setMaxLod(FLT_MAX);
	auto format = vk::Format::eR8G8B8A8Srgb;

	if (p_textures.empty() && textures.empty()) {
		nvvk::Texture texture;

		std::array<uint8_t, 4> color{ 255u, 255u, 255u, 255u };
		vk::DeviceSize buffer_size = sizeof(color);
		auto img_size = vk::Extent2D(1, 1);
		auto image_info = nvvk::makeImage2DCreateInfo(img_size, format);

		auto image = alloc.createImage(cmd_buf, buffer_size, color.data(), image_info);
		auto view_info = nvvk::makeImageViewCreateInfo(image.image, image_info);
		texture = alloc.createTexture(image, view_info, sampler_info);

		nvvk::cmdBarrierImageLayout(
				cmd_buf, texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
		textures.push_back(texture);
	} else {
		for (const auto &texture : p_textures) {
			std::stringstream o;

			int tex_width, tex_height, tex_channels;

			o << "media/textures/" << texture;
			std::string txt_file = nvh::findFile(o.str(), default_search_paths);

			stbi_uc *stbi_pixels = stbi_load(txt_file.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

			std::array<stbi_uc, 4> color{ 255u, 0u, 255u, 255u };

			stbi_uc *pixels = stbi_pixels;

			if (!stbi_pixels) {
				tex_width = tex_height = 1;
				tex_channels = 4;
				pixels = reinterpret_cast<stbi_uc *>(color.data());
			}

			vk::DeviceSize buffer_size = static_cast<uint64_t>(tex_width) * tex_height * sizeof(uint8_t) * 4;
			auto img_size = vk::Extent2D(tex_width, tex_height);
			auto image_info = nvvk::makeImage2DCreateInfo(img_size, format, vkIU::eSampled, true);

			{
				nvvk::ImageDedicated image = alloc.createImage(cmd_buf, buffer_size, pixels, image_info);
				nvvk::cmdGenerateMipmaps(cmd_buf, image.image, format, img_size, image_info.mipLevels);
				auto view_info = nvvk::makeImageViewCreateInfo(image.image, image_info);
				auto tex = alloc.createTexture(image, view_info, sampler_info);

				textures.push_back(tex);
			}

			stbi_image_free(stbi_pixels);
		}
	}
}

void Opal::loadModel(const std::string &filename, nvmath::mat4f transform) {
	using vkBU = vk::BufferUsageFlagBits;

	ObjLoader loader;
	loader.loadModel(filename);

	for (auto &m : loader.materials) {
		m.ambient = nvmath::pow(m.ambient, 2.2f);
		m.diffuse = nvmath::pow(m.diffuse, 2.2f);
		m.specular = nvmath::pow(m.specular, 2.2f);
	}

	ObjInstance instance;
	instance.object_index = static_cast<uint32_t>(object_models.size());
	instance.transform = transform;
	instance.transform_inverse = nvmath::transpose(nvmath::invert(transform));
	instance.texture_offset = static_cast<uint32_t>(textures.size());

	ObjModel model;
	model.indices = static_cast<uint32_t>(loader.indices.size());
	model.vertices = static_cast<uint32_t>(loader.vertices.size());

	nvvk::CommandPool cmd_buf_get(m_device, m_graphicsQueueIndex);
	vk::CommandBuffer cmd_buf = cmd_buf_get.createCommandBuffer();
	model.vertex_buffer = alloc.createBuffer(
			cmd_buf, loader.vertices, vkBU::eVertexBuffer | vkBU::eStorageBuffer | vkBU::eShaderDeviceAddress);
	model.index_buffer = alloc.createBuffer(
			cmd_buf, loader.indices, vkBU::eIndexBuffer | vkBU::eStorageBuffer | vkBU::eShaderDeviceAddress);
	model.mat_color_buffer = alloc.createBuffer(cmd_buf, loader.materials, vkBU::eStorageBuffer);
	model.mat_index_buffer = alloc.createBuffer(cmd_buf, loader.mat_index, vkBU::eStorageBuffer);

	createTextureImages(cmd_buf, loader.textures);
	cmd_buf_get.submitAndWait(cmd_buf);
	alloc.finalizeAndReleaseStaging();

	auto obj_nb = std::to_string(instance.object_index);
	debug.setObjectName(model.vertex_buffer.buffer, std::string("vertex_" + obj_nb).c_str());
	debug.setObjectName(model.index_buffer.buffer, std::string("index_" + obj_nb).c_str());
	debug.setObjectName(model.mat_color_buffer.buffer, std::string("mat_" + obj_nb).c_str());
	debug.setObjectName(model.mat_index_buffer.buffer, std::string("mat_index_" + obj_nb).c_str());

	object_models.emplace_back(model);
	object_instances.emplace_back(instance);
}

void Opal::resetFrame() {
	rt_push_constants.frame = -1;
}

void Opal::updateFrame() {
	static nvmath::mat4f prev_cam_matrix;

	// get camera matrix
	auto &m = CameraManip.getMatrix();

	// if camera has moved
	if (memcmp(&prev_cam_matrix.a00, &m.a00, sizeof(nvmath::mat4f)) != 0) {
		resetFrame();
		prev_cam_matrix = m;
	}

	rt_push_constants.frame++;
}

void Opal::render() {

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// update camera buffer
	updateUniformBuffer();

	// render UI
	{
		bool changed = false;

		changed |= ImGui::ColorEdit3("Clear color", reinterpret_cast<float *>(&clear_color));
		changed |= ImGui::Checkbox("Ray Tracer mode", &raytracing_enabled);
		changed |= ImGui::InputInt("Max Accumulation Frames", &max_frames);
		max_frames = std::max(max_frames, 1);

		static int item = 1;

		if (ImGui::Combo("Up Vector", &item, "X\0Y\0Z\0\0")) {
			nvmath::vec3f pos, eye, up;
			CameraManip.getLookat(pos, eye, up);
			up = nvmath::vec3f(item == 0, item == 1, item == 2);
			CameraManip.setLookat(pos, eye, up);
			changed = true;
		}

		changed |= ImGui::SliderFloat3("Light Position", &push_constant.light_position.x, -20.f, 20.f);
		changed |= ImGui::SliderFloat("Light Intensity", &push_constant.light_intensity, 0.f, 100.f);
		changed |= ImGui::RadioButton("Point", &push_constant.light_type, 0);
		ImGui::SameLine();
		changed |= ImGui::RadioButton("Infinite", &push_constant.light_type, 1);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
				1000.0f / ImGui::GetIO().Framerate,
				ImGui::GetIO().Framerate);
		ImGui::Render();

		if (changed) {
			resetFrame();
		}
	}

	// queue up next frame
	prepareFrame();

	// get frame index
	auto frame_index = getCurFrame();

	// get command buffer for current frame
	const auto &cmd_buf = getCommandBuffers()[frame_index];

	// start command buffer
	cmd_buf.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	// render scene

	clear_color_values[0].setColor(
			std::array<float, 4>({ clear_color[0], clear_color[1], clear_color[2], clear_color[3] }));
	clear_color_values[1].setDepthStencil({ 1.f, 0 });

	if (raytracing_enabled) {
		// raytrace scene objects
		raytrace(cmd_buf);
	} else {

		vk::RenderPassBeginInfo offscreen_pass_info;
		offscreen_pass_info.setClearValueCount(2);
		offscreen_pass_info.setPClearValues(clear_color_values);
		offscreen_pass_info.setRenderPass(offscreen_render_pass);
		offscreen_pass_info.setFramebuffer(offscreen_framebuffer);
		offscreen_pass_info.setRenderArea({ {}, m_size });

		cmd_buf.beginRenderPass(offscreen_pass_info, vk::SubpassContents::eInline);

		// rasterize scene objects
		rasterize(cmd_buf);

		cmd_buf.endRenderPass();
	}

	{
		vk::RenderPassBeginInfo post_pass_info;
		post_pass_info.setClearValueCount(2);
		post_pass_info.setPClearValues(clear_color_values);
		post_pass_info.setRenderPass(getRenderPass());
		post_pass_info.setFramebuffer(getFramebuffers()[frame_index]);
		post_pass_info.setRenderArea({ {}, m_size });

		cmd_buf.beginRenderPass(post_pass_info, vk::SubpassContents::eInline);

		drawPost(cmd_buf);

		ImGui::RenderDrawDataVK(cmd_buf, ImGui::GetDrawData());

		cmd_buf.endRenderPass();
	}

	// commit commands
	cmd_buf.end();

	// commit frame
	submitFrame();
}

void Opal::onResize(int, int) {
	resetFrame();

	createOffscreenRender();
	updatePostDescriptorSet();
	updateRtDescriptorSet();
}

void Opal::rasterize(const vk::CommandBuffer &cmd_buf) {

	using BP = vk::PipelineBindPoint;
	using SS = vk::ShaderStageFlagBits;
	vk::DeviceSize offset{ 0 };

	debug.beginLabel(cmd_buf, "Rasterize");

	cmd_buf.setViewport(0, { vk::Viewport(0, 0, (float)m_size.width, (float)m_size.height, 0, 1) });
	cmd_buf.setScissor(0, { { { 0, 0 }, { m_size.width, m_size.height } } });

	cmd_buf.bindPipeline(BP::eGraphics, graphics_pipeline);
	cmd_buf.bindDescriptorSets(BP::eGraphics, pipeline_layout, 0, { descriptor_set }, {});

	for (int i = 0; i < object_instances.size(); ++i) {
		auto &instance = object_instances[i];
		auto &model = object_models[instance.object_index];
		push_constant.instance_id = i;

		cmd_buf.pushConstants<ObjPushConstant>(pipeline_layout, SS::eVertex | SS::eFragment, 0, push_constant);
		cmd_buf.bindVertexBuffers(0, { model.vertex_buffer.buffer }, { offset });
		cmd_buf.bindIndexBuffer(model.index_buffer.buffer, 0, vk::IndexType::eUint32);
		cmd_buf.drawIndexed(model.indices, 1, 0, 0, 0);
	}

	debug.endLabel(cmd_buf);
}

void Opal::raytrace(const vk::CommandBuffer &cmd_buf) {

	updateFrame();
	if (rt_push_constants.frame >= max_frames) {
		return;
	}

	debug.beginLabel(cmd_buf, "Raytrace");

	// update push constants
	rt_push_constants.clear_color = clear_color;
	rt_push_constants.light_position = push_constant.light_position;
	rt_push_constants.light_intensity = push_constant.light_intensity;
	rt_push_constants.light_type = push_constant.light_type;

	cmd_buf.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, rt_graphics_pipeline);
	cmd_buf.bindDescriptorSets(
			vk::PipelineBindPoint::eRayTracingKHR, rt_pipeline_layout, 0, { rt_descriptor_set, descriptor_set }, {});

	// send push constants
	cmd_buf.pushConstants<RtPushConstant>(rt_pipeline_layout,
			vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
					vk::ShaderStageFlagBits::eMissKHR,
			0,
			rt_push_constants);

	vk::DeviceSize prog_size = rt_properties.shaderGroupBaseAlignment;
	vk::DeviceSize raygen_offset = 0u * prog_size;
	vk::DeviceSize miss_offset = 1u * prog_size;
	vk::DeviceSize hit_group_offset = 3u * prog_size;

	vk::DeviceSize sbt_size = prog_size * (vk::DeviceSize)rt_shader_groups.size();

	const vk::StridedBufferRegionKHR raygen_sbt = { rt_sbt_buffer.buffer, raygen_offset, prog_size, sbt_size };
	const vk::StridedBufferRegionKHR miss_sbt = { rt_sbt_buffer.buffer, miss_offset, prog_size, sbt_size };
	const vk::StridedBufferRegionKHR hit_sbt = { rt_sbt_buffer.buffer, hit_group_offset, prog_size, sbt_size };
	const vk::StridedBufferRegionKHR callable_sbt;

	cmd_buf.traceRaysKHR(&raygen_sbt, &miss_sbt, &hit_sbt, &callable_sbt, m_size.width, m_size.height, 1);

	debug.endLabel(cmd_buf);
}

void Opal::drawPost(const vk::CommandBuffer &cmd_buf) {
	debug.beginLabel(cmd_buf, "Post");

	cmd_buf.setViewport(0, { vk::Viewport(0, 0, (float)m_size.width, (float)m_size.height, 0, 1) });
	cmd_buf.setScissor(0, { { { 0, 0 }, { m_size.width, m_size.height } } });

	auto aspect = static_cast<float>(m_size.width) / static_cast<float>(m_size.height);
	cmd_buf.pushConstants<float>(post_pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, aspect);
	cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, post_graphics_pipeline);
	cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, post_pipeline_layout, 0, post_descriptor_set, {});
	cmd_buf.draw(3, 1, 0, 0);

	debug.endLabel(cmd_buf);
}

// post render pass

void Opal::createOffscreenRender() {
	alloc.destroy(offscreen_color_texture);
	alloc.destroy(offscreen_depth_texture);

	// create offscreen color buffer
	{
		auto color_create_info = nvvk::makeImage2DCreateInfo(m_size,
				offscreen_color_format,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
						vk::ImageUsageFlagBits::eStorage);
		auto image = alloc.createImage(color_create_info);

		offscreen_color_texture = alloc.createTexture(
				image, nvvk::makeImageViewCreateInfo(image.image, color_create_info), vk::SamplerCreateInfo());
		offscreen_color_texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// create offscreen depth buffer
	{
		auto depth_create_info = nvvk::makeImage2DCreateInfo(
				m_size, offscreen_depth_format, vk::ImageUsageFlagBits::eDepthStencilAttachment);
		auto image = alloc.createImage(depth_create_info);

		vk::ImageViewCreateInfo view_info;
		view_info.setViewType(vk::ImageViewType::e2D);
		view_info.setFormat(offscreen_depth_format);
		view_info.setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
		view_info.setImage(image.image);

		offscreen_depth_texture = alloc.createTexture(image, view_info);
	}

	// set layout for both buffers
	{
		nvvk::CommandPool gen_cmd_buf(m_device, m_graphicsQueueIndex);
		auto cmd_buf = gen_cmd_buf.createCommandBuffer();

		nvvk::cmdBarrierImageLayout(
				cmd_buf, offscreen_color_texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
		nvvk::cmdBarrierImageLayout(cmd_buf,
				offscreen_depth_texture.image,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eDepthStencilAttachmentOptimal,
				vk::ImageAspectFlagBits::eDepth);

		gen_cmd_buf.submitAndWait(cmd_buf);
	}

	// create render pass if it is null

	if (!offscreen_render_pass) {
		offscreen_render_pass = nvvk::createRenderPass(m_device,
				{ offscreen_color_format },
				offscreen_depth_format,
				1,
				true,
				true,
				vk::ImageLayout::eGeneral,
				vk::ImageLayout::eGeneral);
	}

	// (re)create offscreen framebuffer

	std::vector<vk::ImageView> attachments = { offscreen_color_texture.descriptor.imageView,
		offscreen_depth_texture.descriptor.imageView };

	// destroy existing framebuffer
	m_device.destroy(offscreen_framebuffer);

	// create new framebuffer
	vk::FramebufferCreateInfo buffer_info;
	buffer_info.setRenderPass(offscreen_render_pass);
	buffer_info.setAttachmentCount(2);
	buffer_info.setPAttachments(attachments.data());
	buffer_info.setWidth(m_size.width);
	buffer_info.setHeight(m_size.height);
	buffer_info.setLayers(1);

	offscreen_framebuffer = m_device.createFramebuffer(buffer_info);
}

void Opal::createPostPipeline() {
	using vkSS = vk::ShaderStageFlagBits;

	vk::PushConstantRange push_constant_ranges = { vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) };

	vk::PipelineLayoutCreateInfo pipeline_info;
	pipeline_info.setSetLayoutCount(1);
	pipeline_info.setPSetLayouts(&post_descriptor_set_layout);
	pipeline_info.setPushConstantRangeCount(1);
	pipeline_info.setPPushConstantRanges(&push_constant_ranges);
	post_pipeline_layout = m_device.createPipelineLayout(pipeline_info);

	nvvk::GraphicsPipelineGeneratorCombined pipeline_generator(m_device, post_pipeline_layout, m_renderPass);
	pipeline_generator.addShader(
			nvh::loadFile("shaders/passthrough.vert.spv", true, default_search_paths, true), vkSS::eVertex);
	pipeline_generator.addShader(
			nvh::loadFile("shaders/post.frag.spv", true, default_search_paths, true), vkSS::eFragment);
	pipeline_generator.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);

	post_graphics_pipeline = pipeline_generator.createPipeline();

	debug.setObjectName(post_graphics_pipeline, "post");
}

void Opal::createPostDescriptor() {
	using vkDS = vk::DescriptorSetLayoutBinding;
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;

	post_descriptor_set_layout_bindings.addBinding(vkDS(0, vkDT::eCombinedImageSampler, 1, vkSS::eFragment));
	post_descriptor_set_layout = post_descriptor_set_layout_bindings.createLayout(m_device);
	post_descriptor_pool = post_descriptor_set_layout_bindings.createPool(m_device);
	post_descriptor_set = nvvk::allocateDescriptorSet(m_device, post_descriptor_pool, post_descriptor_set_layout);
}

void Opal::updatePostDescriptorSet() {
	vk::WriteDescriptorSet write_descriptor_sets =
			post_descriptor_set_layout_bindings.makeWrite(post_descriptor_set, 0, &offscreen_color_texture.descriptor);
	m_device.updateDescriptorSets(write_descriptor_sets, nullptr);
}

void Opal::createGraphicsPipeline() {
	using vkSS = vk::ShaderStageFlagBits;

	vk::PushConstantRange push_constant_ranges = { vkSS::eVertex | vkSS::eFragment, 0, sizeof(ObjPushConstant) };

	vk::PipelineLayoutCreateInfo pipeline_info;
	vk::DescriptorSetLayout desc_set_layout(descriptor_set_layout);
	pipeline_info.setSetLayoutCount(1);
	pipeline_info.setPSetLayouts(&desc_set_layout);
	pipeline_info.setPushConstantRangeCount(1);
	pipeline_info.setPPushConstantRanges(&push_constant_ranges);

	pipeline_layout = m_device.createPipelineLayout(pipeline_info);

	nvvk::GraphicsPipelineGeneratorCombined pipeline_generator(m_device, pipeline_layout, offscreen_render_pass);
	pipeline_generator.depthStencilState.depthTestEnable = true;
	pipeline_generator.addShader(
			nvh::loadFile("shaders/vert_shader.vert.spv", true, default_search_paths, true), vkSS::eVertex);
	pipeline_generator.addShader(
			nvh::loadFile("shaders/frag_shader.frag.spv", true, default_search_paths, true), vkSS::eFragment);
	pipeline_generator.addBindingDescription({ 0, sizeof(VertexObj) });
	pipeline_generator.addAttributeDescriptions(std::vector<vk::VertexInputAttributeDescription>{
			{ 0, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(VertexObj, position)) },
			{ 1, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(VertexObj, normal)) },
			{ 2, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(VertexObj, color)) },
			{ 3, 0, vk::Format::eR32G32Sfloat, static_cast<uint32_t>(offsetof(VertexObj, tex_coord)) },
	});

	graphics_pipeline = pipeline_generator.createPipeline();

	debug.setObjectName(graphics_pipeline, "Graphics");
}

void Opal::createDescriptorSetLayout() {
	using vkDS = vk::DescriptorSetLayoutBinding;
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	uint32_t nb_texture_count = static_cast<uint32_t>(textures.size());
	uint32_t nb_object_model_count = static_cast<uint32_t>(object_models.size());

	uint32_t nb_volume_count = static_cast<uint32_t>(volumes.size());
	uint32_t nb_volume_density_textures_count = static_cast<uint32_t>(volume_density_textures.size());

	// camera matrices (binding = 0)
	descriptor_set_layout_bindings.addBinding(vkDS(0, vkDT::eUniformBuffer, 1, vkSS::eVertex | vkSS::eRaygenKHR));
	// materials (binding = 1)
	descriptor_set_layout_bindings.addBinding(vkDS(
			1, vkDT::eStorageBuffer, nb_object_model_count, vkSS::eVertex | vkSS::eFragment | vkSS::eClosestHitKHR));
	// scene description (binding = 2)
	descriptor_set_layout_bindings.addBinding(
			vkDS(2, vkDT::eStorageBuffer, 1, vkSS::eVertex | vkSS::eFragment | vkSS::eClosestHitKHR));
	// textures (binding = 3)
	descriptor_set_layout_bindings.addBinding(
			vkDS(3, vkDT::eCombinedImageSampler, nb_texture_count, vkSS::eFragment | vkSS::eClosestHitKHR));
	// materials (binding = 4)
	descriptor_set_layout_bindings.addBinding(
			vkDS(4, vkDT::eStorageBuffer, nb_object_model_count, vkSS::eFragment | vkSS::eClosestHitKHR));
	// storing vertices (binding = 5)
	descriptor_set_layout_bindings.addBinding(
			vkDS(5, vkDT::eStorageBuffer, nb_object_model_count, vkSS::eClosestHitKHR));
	// storing indices (binding = 6)
	descriptor_set_layout_bindings.addBinding(
			vkDS(6, vkDT::eStorageBuffer, nb_object_model_count, vkSS::eClosestHitKHR));

	// // storing spheres (binding = 7)
	// descriptor_set_layout_bindings.addBinding(
	// 		vkDS(7, vkDT::eStorageBuffer, 1, vkSS::eClosestHitKHR | vkSS::eIntersectionKHR));

	// storing volumes (binding = 7)
	descriptor_set_layout_bindings.addBinding(
			vkDS(7, vkDT::eStorageBuffer, 1, vkSS::eClosestHitKHR | vkSS::eIntersectionKHR));

	// storing volume density textures (binding = 8)
	descriptor_set_layout_bindings.addBinding(
			vkDS(8, vkDT::eCombinedImageSampler, nb_volume_density_textures_count, vkSS::eIntersectionKHR));

	descriptor_set_layout = descriptor_set_layout_bindings.createLayout(m_device);
	descriptor_pool = descriptor_set_layout_bindings.createPool(m_device, 1);
	descriptor_set = nvvk::allocateDescriptorSet(m_device, descriptor_pool, descriptor_set_layout);
}

void Opal::createUniformBuffer() {
	using vkBU = vk::BufferUsageFlagBits;
	using vkMP = vk::MemoryPropertyFlagBits;

	camera_mat =
			alloc.createBuffer(sizeof(CameraMatrices), vkBU::eUniformBuffer, vkMP::eHostVisible | vkMP::eHostCoherent);

	debug.setObjectName(camera_mat.buffer, "camera_mat");
}

void Opal::createSceneDescriptionBuffer() {
	using vkBU = vk::BufferUsageFlagBits;
	nvvk::CommandPool cmd_gen(m_device, m_graphicsQueueIndex);

	auto cmd_buf = cmd_gen.createCommandBuffer();
	scene_descriptor = alloc.createBuffer(cmd_buf, object_instances, vkBU::eStorageBuffer);
	cmd_gen.submitAndWait(cmd_buf);
	alloc.finalizeAndReleaseStaging();

	debug.setObjectName(scene_descriptor.buffer, "scene_descriptor");
}

void Opal::updateDescriptorSet() {
	std::vector<vk::WriteDescriptorSet> writes;

	// camera
	vk::DescriptorBufferInfo dbi_cam{ camera_mat.buffer, 0, VK_WHOLE_SIZE };
	writes.emplace_back(descriptor_set_layout_bindings.makeWrite(descriptor_set, 0, &dbi_cam));

	// scene
	vk::DescriptorBufferInfo dbi_scene{ scene_descriptor.buffer, 0, VK_WHOLE_SIZE };
	writes.emplace_back(descriptor_set_layout_bindings.makeWrite(descriptor_set, 2, &dbi_scene));

	// materials
	std::vector<vk::DescriptorBufferInfo> vert_info;
	std::vector<vk::DescriptorBufferInfo> index_info;
	std::vector<vk::DescriptorBufferInfo> mat_color_info;
	std::vector<vk::DescriptorBufferInfo> mat_index_info;
	for (auto obj : object_models) {
		vert_info.push_back({ obj.vertex_buffer.buffer, 0, VK_WHOLE_SIZE });
		index_info.push_back({ obj.index_buffer.buffer, 0, VK_WHOLE_SIZE });
		mat_color_info.push_back({ obj.mat_color_buffer.buffer, 0, VK_WHOLE_SIZE });
		mat_index_info.push_back({ obj.mat_index_buffer.buffer, 0, VK_WHOLE_SIZE });
	}

	// sphere materials
	// mat_color_info.emplace_back(spheres_mat_color_buffer.buffer, 0, VK_WHOLE_SIZE);
	// mat_index_info.emplace_back(spheres_mat_index_buffer.buffer, 0, VK_WHOLE_SIZE);

	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 1, mat_color_info.data()));
	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 4, mat_index_info.data()));
	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 5, vert_info.data()));
	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 6, index_info.data()));

	// // write sphere buffer
	// vk::DescriptorBufferInfo spheres_info{ spheres_buffer.buffer, 0, VK_WHOLE_SIZE };
	// writes.emplace_back(descriptor_set_layout_bindings.makeWrite(descriptor_set, 7, &spheres_info));

	// write volumes buffer
	vk::DescriptorBufferInfo volumes_info{ volumes_buffer.buffer, 0, VK_WHOLE_SIZE };
	writes.emplace_back(descriptor_set_layout_bindings.makeWrite(descriptor_set, 7, &volumes_info));

	// volume density textures
	std::vector<vk::DescriptorImageInfo> density_image_info;
	for (auto &texture : volume_density_textures)
		density_image_info.push_back(texture.descriptor);
	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 8, density_image_info.data()));

	// textures
	std::vector<vk::DescriptorImageInfo> image_info;
	for (auto &texture : textures)
		image_info.push_back(texture.descriptor);
	writes.emplace_back(descriptor_set_layout_bindings.makeWriteArray(descriptor_set, 3, image_info.data()));

	// write to device
	m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void Opal::updateUniformBuffer() {
	const float aspect = m_size.width / static_cast<float>(m_size.height);

	CameraMatrices ubo = {};
	ubo.view = CameraManip.getMatrix();
	ubo.proj = nvmath::perspectiveVK(CameraManip.getFov(), aspect, 0.1f, 1000.0f);
	ubo.view_inverse = nvmath::invert(ubo.view);
	ubo.proj_inverse = nvmath::invert(ubo.proj);

	void *data = m_device.mapMemory(camera_mat.allocation, 0, sizeof(ubo));
	memcpy(data, &ubo, sizeof(ubo));
	m_device.unmapMemory(camera_mat.allocation);
}

void Opal::initRayTracing() {
	auto properties =
			m_physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPropertiesKHR>();

	rt_properties = properties.get<vk::PhysicalDeviceRayTracingPropertiesKHR>();
	rt_builder.setup(m_device, &alloc, m_graphicsQueueIndex);
}

nvvk::RaytracingBuilderKHR::Blas Opal::objectToVkGeometryKHR(const ObjModel &model) {

	// acceleration structure info
	vk::AccelerationStructureCreateGeometryTypeInfoKHR as_info;
	as_info.setGeometryType(vk::GeometryTypeKHR::eTriangles);
	as_info.setIndexType(vk::IndexType::eUint32);
	as_info.setVertexFormat(vk::Format::eR32G32B32A32Sfloat);
	// indices / 3 = triangle count
	as_info.setMaxPrimitiveCount(model.indices / 3);
	as_info.setMaxVertexCount(model.vertices);
	// no adding transformation matrices
	as_info.setAllowsTransforms(VK_FALSE);

	vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
	triangles.setVertexFormat(as_info.vertexFormat);
	triangles.setIndexType(as_info.indexType);
	triangles.setVertexData(m_device.getBufferAddress({ model.vertex_buffer.buffer }));
	triangles.setIndexData(m_device.getBufferAddress({ model.index_buffer.buffer }));
	triangles.setVertexStride(sizeof(VertexObj));
	triangles.setTransformData({});

	vk::AccelerationStructureGeometryKHR as_geometry;
	as_geometry.setGeometryType(as_info.geometryType);
	as_geometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
	as_geometry.geometry.setTriangles(triangles);

	vk::AccelerationStructureBuildOffsetInfoKHR offset;
	offset.setFirstVertex(0);
	offset.setPrimitiveCount(as_info.maxPrimitiveCount);
	offset.setPrimitiveOffset(0);
	offset.setTransformOffset(0);

	nvvk::RaytracingBuilderKHR::Blas blas;
	blas.asCreateGeometryInfo.emplace_back(as_info);
	blas.asGeometry.emplace_back(as_geometry);
	blas.asBuildOffsetInfo.emplace_back(offset);

	return blas;
}

void Opal::createSpheres() {
	std::random_device rd{};
	std::mt19937 gen{ rd() };
	std::normal_distribution<float> xzd{ 0.f, 5.f };
	std::normal_distribution<float> yd{ 3.f, 1.f };
	std::uniform_real_distribution<float> radd{ .05f, .2f };

	Sphere s;
	for (uint32_t i = 0; i < 2; i++) {
		s.center = nvmath::vec3f(xzd(gen), yd(gen), xzd(gen));
		s.radius = radd(gen);
		spheres.emplace_back(s);
	}

	std::vector<Aabb> aabbs;
	for (const auto &s : spheres) {
		Aabb aabb;
		aabb.minimum = s.center - nvmath::vec3f(s.radius);
		aabb.maximum = s.center + nvmath::vec3f(s.radius);
		aabbs.emplace_back(aabb);
	}

	// create two materials
	MaterialObj mat;
	mat.diffuse = vec3f(0, 1, 1);
	std::vector<MaterialObj> materials;
	std::vector<int> mat_idx;
	materials.emplace_back(mat);
	mat.diffuse = vec3f(1, 1, 0);
	materials.emplace_back(mat);

	// assign sphere materials
	for (size_t i = 0; i < spheres.size(); i++) {
		// mat_idx.push_back(i % 2);
		mat_idx.push_back(0);
	}

	// create the buffers

	using vkBU = vk::BufferUsageFlagBits;
	nvvk::CommandPool gen_cmd_buf(m_device, m_graphicsQueueIndex);
	auto cmd_buf = gen_cmd_buf.createCommandBuffer();

	spheres_buffer = alloc.createBuffer(cmd_buf, spheres, vkBU::eStorageBuffer);
	spheres_aabb_buffer = alloc.createBuffer(cmd_buf, aabbs, vkBU::eShaderDeviceAddress);
	spheres_mat_index_buffer = alloc.createBuffer(cmd_buf, mat_idx, vkBU::eStorageBuffer);
	spheres_mat_color_buffer = alloc.createBuffer(cmd_buf, materials, vkBU::eStorageBuffer);
	gen_cmd_buf.submitAndWait(cmd_buf);

	debug.setObjectName(spheres_buffer.buffer, "spheres");
	debug.setObjectName(spheres_aabb_buffer.buffer, "spheresAabb");
	debug.setObjectName(spheres_mat_color_buffer.buffer, "spheresMat");
	debug.setObjectName(spheres_mat_index_buffer.buffer, "spheresMatIdx");
}

nvvk::RaytracingBuilderKHR::Blas Opal::sphereToVkGeometryKHR() {

	vk::AccelerationStructureCreateGeometryTypeInfoKHR as_info;
	as_info.setGeometryType(vk::GeometryTypeKHR::eAabbs);
	as_info.setMaxPrimitiveCount((uint32_t)spheres.size());
	as_info.setIndexType(vk::IndexType::eNoneKHR);
	as_info.setVertexFormat(vk::Format::eUndefined);
	as_info.setMaxVertexCount(0);
	as_info.setAllowsTransforms(VK_FALSE);

	auto data_address = m_device.getBufferAddress({ spheres_aabb_buffer.buffer });
	vk::AccelerationStructureGeometryAabbsDataKHR aabbs;
	aabbs.setData(data_address);
	aabbs.setStride(sizeof(Aabb));

	vk::AccelerationStructureGeometryKHR as_geom;
	as_geom.setGeometryType(as_info.geometryType);
	as_geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
	as_geom.geometry.setAabbs(aabbs);

	vk::AccelerationStructureBuildOffsetInfoKHR offset;
	offset.setFirstVertex(0);
	offset.setPrimitiveCount(as_info.maxPrimitiveCount);
	offset.setPrimitiveOffset(0);
	offset.setTransformOffset(0);

	nvvk::RaytracingBuilderKHR::Blas blas;
	blas.asGeometry.emplace_back(as_geom);
	blas.asCreateGeometryInfo.emplace_back(as_info);
	blas.asBuildOffsetInfo.emplace_back(offset);

	return blas;
}

void Opal::createVolumes() {
	std::random_device rd{};
	std::mt19937 gen{ rd() };
	std::normal_distribution<float> xzd{ 0.f, 5.f };
	std::normal_distribution<float> yd{ 3.f, 1.f };
	std::uniform_real_distribution<float> radd{ .05f, .2f };

	VolumeInstance v;
	Aabb aabb;

	std::vector<Aabb> aabbs;

	int count = 3;
	float size = 4.f;
	float spacing = 1.f;

	for (uint32_t i = 0; i < count * count; i++) {
		// v.position = nvmath::vec3f(xzd(gen), yd(gen), xzd(gen));
		v.position = nvmath::vec3f(i % count, 0.f, i / count) * (size + spacing);
		v.position -= nvmath::vec3f(1.f, 0.f, 1.f) * (count / 2 * (size + spacing) + (size * 0.5f));

		v.size = nvmath::vec3f(size);
		v.density_texture_id = 0;
		volumes.emplace_back(v);

		aabb.minimum = v.position;
		aabb.maximum = v.position + v.size;
		aabbs.emplace_back(aabb);
	}

	using vkBU = vk::BufferUsageFlagBits;
	nvvk::CommandPool gen_cmd_buf(m_device, m_graphicsQueueIndex);
	auto cmd_buf = gen_cmd_buf.createCommandBuffer();

	volumes_buffer = alloc.createBuffer(cmd_buf, volumes, vkBU::eStorageBuffer);
	volumes_aabb_buffer = alloc.createBuffer(cmd_buf, aabbs, vkBU::eShaderDeviceAddress);

	createVolumeTextureImage(cmd_buf);

	gen_cmd_buf.submitAndWait(cmd_buf);

	debug.setObjectName(volumes_buffer.buffer, "volumes");
	debug.setObjectName(volumes_aabb_buffer.buffer, "volumesAabb");
}

void Opal::createVolumeTextureImage(const vk::CommandBuffer &cmd_buf) {
	using vkIU = vk::ImageUsageFlagBits;

	vk::SamplerCreateInfo sampler_info{
		{}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear
	};
	sampler_info.setMaxLod(FLT_MAX);
	auto format = vk::Format::eR8G8B8A8Srgb;

	// nvvk::Texture texture;

	// std::array<uint8_t, 4> color{ 255u, 0u, 255u, 255u };
	// vk::DeviceSize buffer_size = sizeof(color);
	// auto img_size = vk::Extent3D(1, 1, 1);
	// auto image_info = nvvk::makeImage3DCreateInfo(img_size, format);

	// auto image = alloc.createImage(cmd_buf, buffer_size, color.data(), image_info);
	// auto view_info = nvvk::makeImageViewCreateInfo(image.image, image_info);
	// texture = alloc.createTexture(image, view_info, sampler_info);

	// nvvk::cmdBarrierImageLayout(
	// 		cmd_buf, texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	// volume_density_textures.push_back(texture);

	int tex_size = 8, tex_channels = 4;

	std::vector<stbi_uc> colors;

	for (int x = 0; x < tex_size; x++) {
		for (int y = 0; y < tex_size; y++) {
			for (int z = 0; z < tex_size; z++) {
				colors.emplace_back(x % 2 == 0 ? 0u : 255u);
				colors.emplace_back(y % 2 == 0 ? 0u : 255u);
				colors.emplace_back(z % 2 == 0 ? 0u : 255u);
				colors.emplace_back((x + y + z) % 2 == 0 ? 0u : 255u);
			}
		}
	}
	stbi_uc *pixels = reinterpret_cast<stbi_uc *>(colors.data());

	vk::DeviceSize buffer_size = static_cast<uint64_t>(tex_size) * tex_size * tex_size * sizeof(uint8_t) * 4;
	auto img_size = vk::Extent3D(tex_size, tex_size, tex_size);
	auto image_info = nvvk::makeImage3DCreateInfo(img_size, format, vkIU::eSampled, true);

	{
		nvvk::ImageDedicated image = alloc.createImage(cmd_buf, buffer_size, pixels, image_info);
		// nvvk::cmdGenerateMipmaps(cmd_buf, image.image, format, img_size, image_info.mipLevels);
		auto view_info = nvvk::makeImageViewCreateInfo(image.image, image_info);
		auto tex = alloc.createTexture(image, view_info, sampler_info);

		volume_density_textures.push_back(tex);

		debug.setObjectName(tex.image, "volumesTexture");
	}
}

nvvk::RaytracingBuilderKHR::Blas Opal::volumeToVkGeometryKHR() {

	vk::AccelerationStructureCreateGeometryTypeInfoKHR as_info;
	as_info.setGeometryType(vk::GeometryTypeKHR::eAabbs);
	as_info.setMaxPrimitiveCount((uint32_t)volumes.size());
	as_info.setIndexType(vk::IndexType::eNoneKHR);
	as_info.setVertexFormat(vk::Format::eUndefined);
	as_info.setMaxVertexCount(0);
	as_info.setAllowsTransforms(VK_FALSE);

	auto data_address = m_device.getBufferAddress({ volumes_aabb_buffer.buffer });
	vk::AccelerationStructureGeometryAabbsDataKHR aabbs;
	aabbs.setData(data_address);
	aabbs.setStride(sizeof(Aabb));

	vk::AccelerationStructureGeometryKHR as_geom;
	as_geom.setGeometryType(as_info.geometryType);
	as_geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
	as_geom.geometry.setAabbs(aabbs);

	vk::AccelerationStructureBuildOffsetInfoKHR offset;
	offset.setFirstVertex(0);
	offset.setPrimitiveCount(as_info.maxPrimitiveCount);
	offset.setPrimitiveOffset(0);
	offset.setTransformOffset(0);

	nvvk::RaytracingBuilderKHR::Blas blas;
	blas.asGeometry.emplace_back(as_geom);
	blas.asCreateGeometryInfo.emplace_back(as_info);
	blas.asBuildOffsetInfo.emplace_back(offset);

	return blas;
}

void Opal::createBottomLevelAS() {

	std::vector<nvvk::RaytracingBuilderKHR::Blas> all_blas;
	all_blas.reserve(object_models.size());

	// add normal object models
	for (const auto &object : object_models) {
		auto blas = objectToVkGeometryKHR(object);
		all_blas.emplace_back(blas);
	}

	// // add sphere objects
	// {
	// 	auto blas = sphereToVkGeometryKHR();
	// 	all_blas.emplace_back(blas);
	// }

	// add volume objects
	{
		auto blas = volumeToVkGeometryKHR();
		all_blas.emplace_back(blas);
	}

	rt_builder.buildBlas(all_blas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
}

void Opal::createTopLevelAS() {

	std::vector<nvvk::RaytracingBuilderKHR::Instance> tlas;
	tlas.reserve(object_instances.size());

	for (int i = 0; i < static_cast<int>(object_instances.size()); i++) {
		nvvk::RaytracingBuilderKHR::Instance ray_info;
		// object transform
		ray_info.transform = object_instances[i].transform;
		ray_info.instanceId = i;
		// gl_InstanceID
		ray_info.blasId = object_instances[i].object_index;
		// all objects in the same hit group
		ray_info.hitGroupId = 0;
		ray_info.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

		tlas.emplace_back(ray_info);
	}

	{
		nvvk::RaytracingBuilderKHR::Instance ray_info;
		ray_info.transform = object_instances[0].transform;
		ray_info.instanceId = static_cast<uint32_t>(tlas.size());
		ray_info.blasId = static_cast<uint32_t>(object_models.size());
		// use hit group 1 for primitives
		ray_info.hitGroupId = 1;
		ray_info.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		tlas.emplace_back(ray_info);
	}

	rt_builder.buildTlas(tlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
}

void Opal::createRtDescriptorSet() {
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	using vkDSLB = vk::DescriptorSetLayoutBinding;

	// top level acceleration structure (TLAS)
	rt_descriptor_set_layout_bindings.addBinding(
			vkDSLB(0, vkDT::eAccelerationStructureKHR, 1, vkSS::eRaygenKHR | vkSS::eClosestHitKHR));

	// output image
	rt_descriptor_set_layout_bindings.addBinding(vkDSLB(1, vkDT::eStorageImage, 1, vkSS::eRaygenKHR));

	rt_descriptor_pool = rt_descriptor_set_layout_bindings.createPool(m_device);
	rt_descriptor_set_layout = rt_descriptor_set_layout_bindings.createLayout(m_device);
	rt_descriptor_set = m_device.allocateDescriptorSets({ rt_descriptor_pool, 1, &rt_descriptor_set_layout })[0];

	vk::AccelerationStructureKHR tlas = rt_builder.getAccelerationStructure();

	vk::WriteDescriptorSetAccelerationStructureKHR as_info;
	as_info.setAccelerationStructureCount(1);
	as_info.setPAccelerationStructures(&tlas);

	vk::DescriptorImageInfo image_info{ {}, offscreen_color_texture.descriptor.imageView, vk::ImageLayout::eGeneral };

	std::vector<vk::WriteDescriptorSet> writes;

	writes.emplace_back(rt_descriptor_set_layout_bindings.makeWrite(rt_descriptor_set, 0, &as_info));
	writes.emplace_back(rt_descriptor_set_layout_bindings.makeWrite(rt_descriptor_set, 1, &image_info));

	m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void Opal::updateRtDescriptorSet() {
	using vkDT = vk::DescriptorType;

	// output buffer
	vk::DescriptorImageInfo image_info{ {}, offscreen_color_texture.descriptor.imageView, vk::ImageLayout::eGeneral };
	vk::WriteDescriptorSet write{ rt_descriptor_set, 1, 0, 1, vkDT::eStorageImage, &image_info };
	m_device.updateDescriptorSets(write, nullptr);
}

void Opal::createRtPipeline() {

	std::vector<vk::PipelineShaderStageCreateInfo> stages;

	// raygen stage

	auto raygen_shader = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytrace.rgen.spv", true, default_search_paths, true));

	vk::RayTracingShaderGroupCreateInfoKHR raygen_group_info{ vk::RayTracingShaderGroupTypeKHR::eGeneral,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eRaygenKHR, raygen_shader, "main" });
	raygen_group_info.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	rt_shader_groups.push_back(raygen_group_info);

	// miss stage

	auto raymiss_shader = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytrace.rmiss.spv", true, default_search_paths, true));

	vk::RayTracingShaderGroupCreateInfoKHR raymiss_group_info{ vk::RayTracingShaderGroupTypeKHR::eGeneral,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eMissKHR, raymiss_shader, "main" });
	raymiss_group_info.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	rt_shader_groups.push_back(raymiss_group_info);

	// shadow miss stage

	auto shadow_miss_shader = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytraceShadow.rmiss.spv", true, default_search_paths, true));

	stages.push_back({ {}, vk::ShaderStageFlagBits::eMissKHR, shadow_miss_shader, "main" });
	raymiss_group_info.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	rt_shader_groups.push_back(raymiss_group_info);

	// hit group 0 - closest hit

	auto ray_closest_hit_shader = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytrace.rchit.spv", true, default_search_paths, true));

	vk::RayTracingShaderGroupCreateInfoKHR hit_group_info{ vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eClosestHitKHR, ray_closest_hit_shader, "main" });
	hit_group_info.setClosestHitShader(static_cast<uint32_t>(stages.size() - 1));

	// hit group 0 - any hit

	// auto ray_any_hit_shader_0 = nvvk::createShaderModule(
	// 		m_device, nvh::loadFile("shaders/raytrace_0.rahit.spv", true, default_search_paths, true));

	// stages.push_back({ {}, vk::ShaderStageFlagBits::eAnyHitKHR, ray_any_hit_shader_0, "main" });
	// hit_group_info.setAnyHitShader(static_cast<uint32_t>(stages.size() - 1));

	rt_shader_groups.push_back(hit_group_info);

	// hit group 1 - closest hit + procedural intersection

	auto ray_closest_hit_shader_2 = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytrace2.rchit.spv", true, default_search_paths, true));

	auto ray_intersection_shader = nvvk::createShaderModule(
			m_device, nvh::loadFile("shaders/raytrace.rint.spv", true, default_search_paths, true));

	vk::RayTracingShaderGroupCreateInfoKHR hit_group_1_info{ vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR,
		VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eClosestHitKHR, ray_closest_hit_shader_2, "main" });
	hit_group_1_info.setClosestHitShader(static_cast<uint32_t>(stages.size() - 1));
	stages.push_back({ {}, vk::ShaderStageFlagBits::eIntersectionKHR, ray_intersection_shader, "main" });
	hit_group_1_info.setIntersectionShader(static_cast<uint32_t>(stages.size() - 1));
	rt_shader_groups.push_back(hit_group_1_info);

	// payload 1

	// auto ray_any_hit_shader_1 = nvvk::createShaderModule(
	// 		m_device, nvh::loadFile("shaders/raytrace_1.rahit.spv", true, default_search_paths, true));

	// // not used by shadow (skipped)
	// hit_group_info.setClosestHitShader(VK_SHADER_UNUSED_KHR);
	// stages.push_back({ {}, vk::ShaderStageFlagBits::eAnyHitKHR, ray_any_hit_shader_1, "main" });
	// hit_group_info.setAnyHitShader(static_cast<uint32_t>(stages.size() - 1));
	// rt_shader_groups.push_back(hit_group_info);

	// create the pipeline

	vk::PipelineLayoutCreateInfo pipeline_layout_info;

	vk::PushConstantRange push_constant_range{ vk::ShaderStageFlagBits::eRaygenKHR |
													   vk::ShaderStageFlagBits::eClosestHitKHR |
													   vk::ShaderStageFlagBits::eMissKHR,
		0,
		sizeof(RtPushConstant) };

	pipeline_layout_info.setPushConstantRangeCount(1);
	pipeline_layout_info.setPPushConstantRanges(&push_constant_range);

	std::vector<vk::DescriptorSetLayout> layouts = { rt_descriptor_set_layout, descriptor_set_layout };
	pipeline_layout_info.setSetLayoutCount(static_cast<uint32_t>(layouts.size()));
	pipeline_layout_info.setPSetLayouts(layouts.data());

	rt_pipeline_layout = m_device.createPipelineLayout(pipeline_layout_info);

	vk::RayTracingPipelineCreateInfoKHR ray_pipeline_info;
	ray_pipeline_info.setStageCount(static_cast<uint32_t>(stages.size()));
	ray_pipeline_info.setPStages(stages.data());
	// 1-raygen, n-miss, n-(hit[+anyhit+intersect])
	ray_pipeline_info.setGroupCount(static_cast<uint32_t>(rt_shader_groups.size()));
	ray_pipeline_info.setPGroups(rt_shader_groups.data());

	// max ray depth
	ray_pipeline_info.setMaxRecursionDepth(2);
	ray_pipeline_info.setLayout(rt_pipeline_layout);

	// set the pipeline
	rt_graphics_pipeline =
			static_cast<const vk::Pipeline &>(m_device.createRayTracingPipelineKHR({}, ray_pipeline_info));

	// cleanup
	m_device.destroy(raygen_shader);
	m_device.destroy(raymiss_shader);
	m_device.destroy(shadow_miss_shader);
	m_device.destroy(ray_closest_hit_shader);
	m_device.destroy(ray_closest_hit_shader_2);
	m_device.destroy(ray_intersection_shader);
	// m_device.destroy(ray_any_hit_shader_0);
	// m_device.destroy(ray_any_hit_shader_1);
}

void Opal::createRtShaderBindingTable() {

	auto group_count = static_cast<uint32_t>(rt_shader_groups.size());
	auto group_handle_size = rt_properties.shaderGroupHandleSize;
	auto base_alignment = rt_properties.shaderGroupBaseAlignment;

	auto sbt_size = group_count * base_alignment;

	std::vector<uint8_t> shader_handle_storage(sbt_size);

	m_device.getRayTracingShaderGroupHandlesKHR(
			rt_graphics_pipeline, 0, group_count, sbt_size, shader_handle_storage.data());

	rt_sbt_buffer = alloc.createBuffer(sbt_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	debug.setObjectName(rt_sbt_buffer.buffer, std::string("SBT").c_str());

	void *mapped = alloc.map(rt_sbt_buffer);
	auto *data = reinterpret_cast<uint8_t *>(mapped);
	for (uint32_t g = 0; g < group_count; g++) {
		memcpy(data, shader_handle_storage.data() + g * group_handle_size, group_handle_size);
		data += base_alignment;
	}

	alloc.unmap(rt_sbt_buffer);
	alloc.finalizeAndReleaseStaging();
}
