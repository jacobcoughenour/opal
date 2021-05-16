#include "renderer.h"

// clang-format off
#define TRY(f) if (f() != VK_SUCCESS) return EXIT_FAILURE;
// clang-format on

using namespace Opal;

static void glfw_error_callback(int error, const char *description) {
	LOG_ERR("GLFW Error %d: %s", error, description);
}

int Renderer::initialize() {

	TRY(volkInitialize);

	TRY(create_window);
	TRY(create_vk_instance);
	TRY(create_surface);
	TRY(create_vk_device);
	TRY(create_swapchain);
	TRY(get_queues);
	TRY(create_render_pass);
	TRY(create_graphics_pipeline);
	TRY(create_framebuffers);
	TRY(create_command_pool);
	TRY(create_command_buffers);
	TRY(create_sync_objects);

	_initialized = true;

	return EXIT_SUCCESS;
}

int Renderer::create_window() {

	// set callback for logging glfw errors
	glfwSetErrorCallback(glfw_error_callback);

	// initialize glfw library
	if (!glfwInit()) {
		return EXIT_FAILURE;
	}

	// disable automatic OpenGL context creation from glfw
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// create window
	_window = glfwCreateWindow(
			INIT_WIDTH, INIT_HEIGHT, WINDOW_TITLE, nullptr, nullptr);

	return EXIT_SUCCESS;
}

int Renderer::create_vk_instance() {
	// start building vulkan instance
	vkb::InstanceBuilder instance_builder;

	instance_builder.set_app_name(VK_APP_NAME)
			.set_engine_name(VK_ENGINE_NAME)
			.require_api_version(VK_REQUIRED_API_VERSION);

#ifdef USE_VALIDATION_LAYERS
	// enable validation layers
	instance_builder.request_validation_layers();

	// set callback for logging vulkan validation messages
	instance_builder.set_debug_callback(
			[](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
					VkDebugUtilsMessageTypeFlagsEXT messageType,
					const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
					void *pUserData) -> VkBool32 {
				LOG("[%s: %s] %s",
						vkb::to_string_message_severity(messageSeverity),
						vkb::to_string_message_type(messageType),
						pCallbackData->pMessage);
				return VK_FALSE;
			});
#endif

	// enable instance extensions
	for (auto &name : VK_INSTANCE_EXTENSIONS) {
		instance_builder.enable_extension(name);
	}

	// build vulkan instance
	auto instance_builder_return = instance_builder.build();

	// handle vulkan instance errors
	if (!instance_builder_return) {
		LOG_ERR("Failed to create Vulkan instance: %s",
				instance_builder_return.error().message());
		return EXIT_FAILURE;
	}

	// get the final vulkan instance
	_vkb_instance = instance_builder_return.value();

	// attach volk to instance
	volkLoadInstance(_vkb_instance.instance);

	return EXIT_SUCCESS;
}

int Renderer::create_surface() {
	_vk_surface = nullptr;
	VkResult err = glfwCreateWindowSurface(
			_vkb_instance.instance, _window, NULL, &_vk_surface);
	if (err != VK_SUCCESS) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int Renderer::create_vk_device() {

	// create device selector
	vkb::PhysicalDeviceSelector device_selector{ _vkb_instance };

	// add device extensions
	device_selector.add_required_extensions(VK_REQUIRED_DEVICE_EXTENSIONS);
	device_selector.add_desired_extensions(VK_OPTIONAL_DEVICE_EXTENSIONS);

	// pick compatible device
	vkb::PhysicalDevice physical_device =
			device_selector.set_minimum_version(VK_DEVICE_MINIMUM_VERSION)
					.set_surface(_vk_surface)
					.select()
					.value();

	vkb::DeviceBuilder device_builder{ physical_device };
	auto device_builder_return = device_builder.build();

	if (!device_builder_return) {
		LOG_ERR("Failed to create Vulkan device: %s",
				device_builder_return.error().message());
		return EXIT_FAILURE;
	}

	// get final vulkan device
	_vkb_device = device_builder_return.value();

	// attach volk to device
	volkLoadDevice(_vkb_device.device);

	return EXIT_SUCCESS;
}

int Renderer::create_swapchain() {

	// (re)build swapchain
	vkb::SwapchainBuilder swapchain_builder{ _vkb_device };
	auto swap_ret = swapchain_builder
							.set_old_swapchain(_vkb_swapchain)
							// swapchain settings
							.build();

	if (!swap_ret) {
		LOG_ERR("Failed to create Vulkan swapchain: %s",
				swap_ret.error().message());
		_vkb_swapchain.swapchain = VK_NULL_HANDLE;
		return EXIT_FAILURE;
	}

	// destroy old swapchain
	vkb::destroy_swapchain(_vkb_swapchain);

	// get final swapchain
	_vkb_swapchain = swap_ret.value();

	return EXIT_SUCCESS;
}

int Renderer::get_queues() {
	auto graphics_queue = _vkb_device.get_queue(vkb::QueueType::graphics);
	if (!graphics_queue.has_value()) {
		LOG_ERR("Failed to get graphics queue: %s",
				graphics_queue.error().message());
		return EXIT_FAILURE;
	}
	_graphics_queue = graphics_queue.value();

	auto present_queue = _vkb_device.get_queue(vkb::QueueType::present);
	if (!present_queue.has_value()) {
		LOG_ERR("Failed to get presentation queue: %s",
				present_queue.error().message());
		return EXIT_FAILURE;
	}
	_present_queue = present_queue.value();

	return EXIT_SUCCESS;
}

int Renderer::create_render_pass() {
	// clang-format off
	VkAttachmentDescription color_attachment = {};
	color_attachment.format			= _vkb_swapchain.image_format;
	color_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment	= 0;
	color_attachment_ref.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_attachment_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
								  | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo pass_info = {};
	pass_info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.attachmentCount	= 1;
	pass_info.pAttachments		= &color_attachment;
	pass_info.subpassCount		= 1;
	pass_info.pSubpasses		= &subpass;
	pass_info.dependencyCount	= 1;
	pass_info.pDependencies		= &dependency;
	// clang-format on

	if (vkCreateRenderPass(
				_vkb_device.device, &pass_info, nullptr, &_render_pass) !=
			VK_SUCCESS) {
		LOG_ERR("Failed to create render pass");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int Renderer::create_graphics_pipeline() {

	auto vert_shader = createShaderModuleFromFile(
			_vkb_device.device, "shaders/vert_shader.vert");
	auto frag_shader = createShaderModuleFromFile(
			_vkb_device.device, "shaders/frag_shader.frag");

	if (vert_shader == VK_NULL_HANDLE || frag_shader == VK_NULL_HANDLE) {
		return EXIT_FAILURE;
	}

	VkPipelineShaderStageCreateInfo vert_stage_info = {};
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = vert_shader;
	vert_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo frag_stage_info = {};
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = frag_shader;
	frag_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info,
		frag_stage_info };

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 0;
	vertex_input_info.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_vkb_swapchain.extent.width;
	viewport.height = (float)_vkb_swapchain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = _vkb_swapchain.extent;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask =
			// RGBA
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	color_blending.blendConstants[0] = 0.0f;
	color_blending.blendConstants[1] = 0.0f;
	color_blending.blendConstants[2] = 0.0f;
	color_blending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 0;
	pipeline_layout_info.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(_vkb_device.device,
				&pipeline_layout_info,
				nullptr,
				&_pipeline_layout) != VK_SUCCESS) {
		LOG_ERR("Failed to create pipeline layout");
		return EXIT_FAILURE;
	}

	std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic_info = {};
	dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount =
			static_cast<uint32_t>(dynamic_states.size());
	dynamic_info.pDynamicStates = dynamic_states.data();

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = &dynamic_info;
	pipeline_info.layout = _pipeline_layout;
	pipeline_info.renderPass = _render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(_vkb_device.device,
				VK_NULL_HANDLE,
				1,
				&pipeline_info,
				nullptr,
				&_graphics_pipeline) != VK_SUCCESS) {
		LOG_ERR("Failed to create graphics pipeline");
		return EXIT_FAILURE;
	}

	// clean up shader modules
	vkDestroyShaderModule(_vkb_device.device, frag_shader, nullptr);
	vkDestroyShaderModule(_vkb_device.device, vert_shader, nullptr);

	return EXIT_SUCCESS;
}

int Renderer::create_framebuffers() {

	_swapchain_images = _vkb_swapchain.get_images().value();
	_swapchain_image_views = _vkb_swapchain.get_image_views().value();

	_framebuffers.resize(_swapchain_image_views.size());

	for (size_t i = 0; i < _swapchain_image_views.size(); i++) {
		VkImageView attachments[] = { _swapchain_image_views[i] };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = _render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = _vkb_swapchain.extent.width;
		framebuffer_info.height = _vkb_swapchain.extent.height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(_vkb_device.device,
					&framebuffer_info,
					nullptr,
					&_framebuffers[i]) != VK_SUCCESS) {
			LOG_ERR("Failed to create framebuffer[%d]", i);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int Renderer::create_command_pool() {

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex =
			_vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	if (vkCreateCommandPool(
				_vkb_device.device, &pool_info, nullptr, &_command_pool) !=
			VK_SUCCESS) {
		LOG_ERR("Failed to create command pool");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int Renderer::create_command_buffers() {

	_command_buffers.resize(_framebuffers.size());

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = _command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = (uint32_t)_command_buffers.size();

	if (vkAllocateCommandBuffers(_vkb_device.device,
				&alloc_info,
				_command_buffers.data()) != VK_SUCCESS) {
		LOG_ERR("Failed to allocate command buffers");
		return EXIT_FAILURE;
	}

	for (size_t i = 0; i < _command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(_command_buffers[i], &begin_info) !=
				VK_SUCCESS) {
			LOG_ERR("Failed to begin command buffer [%d]", i);
			return EXIT_FAILURE;
		}

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = _render_pass;
		render_pass_info.framebuffer = _framebuffers[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = _vkb_swapchain.extent;

		VkClearValue clear_color{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clear_color;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)_vkb_swapchain.extent.width;
		viewport.height = (float)_vkb_swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = _vkb_swapchain.extent;

		vkCmdSetViewport(_command_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(_command_buffers[i], 0, 1, &scissor);
		vkCmdBeginRenderPass(_command_buffers[i],
				&render_pass_info,
				VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(_command_buffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				_graphics_pipeline);
		vkCmdDraw(_command_buffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(_command_buffers[i]);

		if (vkEndCommandBuffer(_command_buffers[i]) != VK_SUCCESS) {
			LOG_ERR("Failed to end command buffer [%d]", i);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int Renderer::create_sync_objects() {

	_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
	_image_in_flight.resize(_vkb_swapchain.image_count, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(_vkb_device.device,
					&semaphore_info,
					nullptr,
					&_available_semaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(_vkb_device.device,
						&semaphore_info,
						nullptr,
						&_finished_semaphores[i]) != VK_SUCCESS ||
				vkCreateFence(_vkb_device.device,
						&fence_info,
						nullptr,
						&_in_flight_fences[i]) != VK_SUCCESS) {
			LOG_ERR("Failed to create sync object [%d]", i);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int Renderer::recreate_swapchain() {
	vkDeviceWaitIdle(_vkb_device.device);

	vkDestroyCommandPool(_vkb_device.device, _command_pool, nullptr);
	for (auto framebuffer : _framebuffers) {
		vkDestroyFramebuffer(_vkb_device.device, framebuffer, nullptr);
	}
	_vkb_swapchain.destroy_image_views(_swapchain_image_views);

	TRY(create_swapchain)
	TRY(create_framebuffers)
	TRY(create_command_pool)
	TRY(create_command_buffers)

	return EXIT_SUCCESS;
}

void Renderer::destroy() {

	// cleanup vulkan

	vkDeviceWaitIdle(_vkb_device.device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(
				_vkb_device.device, _finished_semaphores[i], nullptr);
		vkDestroySemaphore(
				_vkb_device.device, _available_semaphores[i], nullptr);
		vkDestroyFence(_vkb_device.device, _in_flight_fences[i], nullptr);
	}

	vkDestroyCommandPool(_vkb_device.device, _command_pool, nullptr);

	for (auto framebuffer : _framebuffers) {
		vkDestroyFramebuffer(_vkb_device.device, framebuffer, nullptr);
	}

	vkDestroyPipeline(_vkb_device.device, _graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(_vkb_device.device, _pipeline_layout, nullptr);
	vkDestroyRenderPass(_vkb_device.device, _render_pass, nullptr);

	_vkb_swapchain.destroy_image_views(_swapchain_image_views);

	vkb::destroy_swapchain(_vkb_swapchain);
	vkb::destroy_device(_vkb_device);
	vkDestroySurfaceKHR(_vkb_instance.instance, _vk_surface, nullptr);
	vkb::destroy_instance(_vkb_instance);

	// cleanup glfw
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void Renderer::start_render_loop() {

	while (!glfwWindowShouldClose(_window)) {
		glfwPollEvents();

		// skip draw if minimized
		if (!glfwGetWindowAttrib(_window, GLFW_VISIBLE))
			continue;

		draw_frame();
	}
}

int Renderer::draw_frame() {

	vkWaitForFences(_vkb_device.device,
			1,
			&_in_flight_fences[_current_frame],
			VK_TRUE,
			UINT64_MAX);

	uint32_t image_index = 0;

	VkResult result = vkAcquireNextImageKHR(_vkb_device.device,
			_vkb_swapchain.swapchain,
			UINT64_MAX,
			_available_semaphores[_current_frame],
			VK_NULL_HANDLE,
			&image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreate_swapchain();
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LOG_ERR("Failed to acquire swapchain image: %s", result);
		return EXIT_FAILURE;
	}

	if (_image_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(_vkb_device.device,
				1,
				&_image_in_flight[image_index],
				VK_TRUE,
				UINT64_MAX);
	}

	_image_in_flight[image_index] = _in_flight_fences[_current_frame];

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { _available_semaphores[_current_frame] };
	VkPipelineStageFlags wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_command_buffers[image_index];

	VkSemaphore signal_semaphores[] = { _finished_semaphores[_current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vkResetFences(_vkb_device.device, 1, &_in_flight_fences[_current_frame]);

	if (vkQueueSubmit(_graphics_queue,
				1,
				&submit_info,
				_in_flight_fences[_current_frame]) != VK_SUCCESS) {
		LOG_ERR("Failed to submit draw command buffer");
		return EXIT_SUCCESS;
	}

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = { _vkb_swapchain.swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_index;
	result = vkQueuePresentKHR(_present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return recreate_swapchain();
	} else if (result != VK_SUCCESS) {
		LOG_ERR("Failed to present swapchain image");
		return EXIT_FAILURE;
	}

	_current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	return EXIT_SUCCESS;
}
