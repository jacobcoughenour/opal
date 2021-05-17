#include "renderer.h"

using namespace Opal;

static void glfw_error_callback(int error, const char *description) {
	LOG_ERR("GLFW Error %d: %s", error, description);
}

Error Renderer::initialize() {

	ERR_FAIL_COND_V_MSG(volkInitialize(), FAIL, "Failed to initialize Volk");

	ERR_TRY(create_window);
	ERR_TRY(create_vk_instance);
	ERR_TRY(create_surface);
	ERR_TRY(create_vk_device);
	ERR_TRY(create_vma_allocator);
	ERR_TRY(create_swapchain);
	ERR_TRY(get_queues);
	ERR_TRY(create_render_pass);
	ERR_TRY(create_graphics_pipeline);
	ERR_TRY(create_framebuffers);
	ERR_TRY(create_command_pool);
	ERR_TRY(create_vertex_buffer);
	ERR_TRY(create_command_buffers);
	ERR_TRY(create_sync_objects);

	_initialized = true;

	return OK;
}

Error Renderer::create_window() {

	// set callback for logging glfw errors
	glfwSetErrorCallback(glfw_error_callback);

	// initialize glfw library
	ERR_FAIL_COND_V_MSG(!glfwInit(), FAIL, "Failed to initialize GLFW");

	// disable automatic OpenGL context creation from glfw
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// create window
	_window =
			glfwCreateWindow(WINDOW_INIT_SIZE, WINDOW_TITLE, nullptr, nullptr);

	return OK;
}

Error Renderer::create_vk_instance() {
	// start building vulkan instance
	vkb::InstanceBuilder instance_builder;

	instance_builder.set_app_name(VK_APP_NAME)
			.set_engine_name(VK_ENGINE_NAME)
			.require_api_version(VK_VERSION_MAJOR(VK_REQUIRED_API_VERSION),
					VK_VERSION_MINOR(VK_REQUIRED_API_VERSION),
					VK_VERSION_PATCH(VK_REQUIRED_API_VERSION));

#ifdef USE_VALIDATION_LAYERS
	// enable validation layers
	instance_builder.request_validation_layers();

	// set callback for logging vulkan validation messages
	instance_builder.set_debug_callback(
			[](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
					VkDebugUtilsMessageTypeFlagsEXT messageType,
					const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
					void *pUserData) -> VkBool32 {
				LOG("VK [%s: %s] %s",
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

	// check instance is valid
	ERR_FAIL_COND_V_MSG(!instance_builder_return,
			FAIL,
			"Failed to create Vulkan instance: %s",
			instance_builder_return.error().message());

	// get the final vulkan instance
	_vkb_instance = instance_builder_return.value();

	// attach volk to instance
	volkLoadInstance(_vkb_instance.instance);

	return OK;
}

Error Renderer::create_surface() {
	_vk_surface = nullptr;
	VkResult err = glfwCreateWindowSurface(
			_vkb_instance.instance, _window, NULL, &_vk_surface);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to create glfw window surface: %s",
			err);

	return OK;
}

Error Renderer::create_vk_device() {

	// create device selector
	vkb::PhysicalDeviceSelector device_selector{ _vkb_instance };

	// add device extensions
	device_selector.add_required_extensions(VK_REQUIRED_DEVICE_EXTENSIONS);
	device_selector.add_desired_extensions(VK_OPTIONAL_DEVICE_EXTENSIONS);

	// pick compatible device
	vkb::PhysicalDevice physical_device =
			device_selector
					.set_minimum_version(
							VK_VERSION_MAJOR(VK_DEVICE_MINIMUM_VERSION),
							VK_VERSION_MINOR(VK_DEVICE_MINIMUM_VERSION))
					.set_surface(_vk_surface)
					.select()
					.value();

	vkb::DeviceBuilder device_builder{ physical_device };
	auto device_builder_return = device_builder.build();

	ERR_FAIL_COND_V_MSG(!device_builder_return,
			FAIL,
			"Failed to create Vulkan device: %s",
			device_builder_return.error().message());

	// get final vulkan device
	_vkb_device = device_builder_return.value();

	// attach volk to device
	volkLoadDevice(_vkb_device.device);

	return OK;
}

Error Renderer::create_vma_allocator() {

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.vulkanApiVersion = VK_REQUIRED_API_VERSION;
	allocator_info.physicalDevice = _vkb_device.physical_device.physical_device;
	allocator_info.device = _vkb_device.device;
	allocator_info.instance = _vkb_instance.instance;

	// get vulkan function pointers from volk

	VolkDeviceTable table;
	volkLoadDeviceTable(&table, _vkb_device.device);

	// remap volk table to vma table
	VmaVulkanFunctions vulkan_funcs = {};
	{
		vulkan_funcs.vkGetPhysicalDeviceProperties =
				vkGetPhysicalDeviceProperties;
		vulkan_funcs.vkGetPhysicalDeviceMemoryProperties =
				vkGetPhysicalDeviceMemoryProperties;
		vulkan_funcs.vkAllocateMemory = table.vkAllocateMemory;
		vulkan_funcs.vkFreeMemory = table.vkFreeMemory;
		vulkan_funcs.vkMapMemory = table.vkMapMemory;
		vulkan_funcs.vkUnmapMemory = table.vkUnmapMemory;
		vulkan_funcs.vkFlushMappedMemoryRanges =
				table.vkFlushMappedMemoryRanges;
		vulkan_funcs.vkInvalidateMappedMemoryRanges =
				table.vkInvalidateMappedMemoryRanges;
		vulkan_funcs.vkBindBufferMemory = table.vkBindBufferMemory;
		vulkan_funcs.vkBindImageMemory = table.vkBindImageMemory;
		vulkan_funcs.vkGetBufferMemoryRequirements =
				table.vkGetBufferMemoryRequirements;
		vulkan_funcs.vkGetImageMemoryRequirements =
				table.vkGetImageMemoryRequirements;
		vulkan_funcs.vkCreateBuffer = table.vkCreateBuffer;
		vulkan_funcs.vkDestroyBuffer = table.vkDestroyBuffer;
		vulkan_funcs.vkCreateImage = table.vkCreateImage;
		vulkan_funcs.vkDestroyImage = table.vkDestroyImage;
		vulkan_funcs.vkCmdCopyBuffer = table.vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
		vulkan_funcs.vkGetBufferMemoryRequirements2KHR =
				table.vkGetBufferMemoryRequirements2KHR;
		vulkan_funcs.vkGetImageMemoryRequirements2KHR =
				table.vkGetImageMemoryRequirements2KHR;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
		vulkan_funcs.vkBindBufferMemory2KHR = table.vkBindBufferMemory2KHR;
		vulkan_funcs.vkBindImageMemory2KHR = table.vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
		vulkan_funcs.vkGetPhysicalDeviceMemoryProperties2KHR =
				vkGetPhysicalDeviceMemoryProperties2KHR;
#endif
	}

	allocator_info.pVulkanFunctions = &vulkan_funcs;

	ERR_FAIL_COND_V_MSG(
			vmaCreateAllocator(&allocator_info, &_vma_allocator) != VK_SUCCESS,
			FAIL,
			"Failed to create Vulkan Memory Allocator");

	return OK;
}

Error Renderer::create_swapchain() {

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
		return FAIL;
	}

	// destroy old swapchain
	vkb::destroy_swapchain(_vkb_swapchain);

	// get final swapchain
	_vkb_swapchain = swap_ret.value();

	return OK;
}

Error Renderer::get_queues() {

	// get graphics queue
	auto graphics_queue = _vkb_device.get_queue(vkb::QueueType::graphics);
	ERR_FAIL_COND_V_MSG(!graphics_queue.has_value(),
			FAIL,
			"Failed to get graphics queue: %s",
			graphics_queue.error().message());
	_graphics_queue = graphics_queue.value();

	// get presentation queue
	auto present_queue = _vkb_device.get_queue(vkb::QueueType::present);
	ERR_FAIL_COND_V_MSG(!present_queue.has_value(),
			FAIL,
			"Failed to get presentation queue: %s",
			present_queue.error().message());
	_present_queue = present_queue.value();

	return OK;
}

Error Renderer::create_render_pass() {
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

	// create render pass
	VkResult err = vkCreateRenderPass(
			_vkb_device.device, &pass_info, nullptr, &_render_pass);
	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create render pass");

	return OK;
}

Error Renderer::create_graphics_pipeline() {

	auto vert_shader = createShaderModuleFromFile(
			_vkb_device.device, "shaders/vert_shader.vert");
	auto frag_shader = createShaderModuleFromFile(
			_vkb_device.device, "shaders/frag_shader.frag");

	if (vert_shader == VK_NULL_HANDLE || frag_shader == VK_NULL_HANDLE) {
		return FAIL;
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

	auto binding_description = Vertex::get_binding_description();
	auto attribute_descriptions = Vertex::get_attribute_descriptions();

	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &binding_description;
	vertex_input_info.vertexAttributeDescriptionCount =
			static_cast<uint32_t>(attribute_descriptions.size());
	vertex_input_info.pVertexAttributeDescriptions =
			attribute_descriptions.data();

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

	VkResult err = vkCreatePipelineLayout(_vkb_device.device,
			&pipeline_layout_info,
			nullptr,
			&_pipeline_layout);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create pipeline layout");

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

	err = vkCreateGraphicsPipelines(_vkb_device.device,
			VK_NULL_HANDLE,
			1,
			&pipeline_info,
			nullptr,
			&_graphics_pipeline);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create graphics pipeline");

	// clean up shader modules
	vkDestroyShaderModule(_vkb_device.device, frag_shader, nullptr);
	vkDestroyShaderModule(_vkb_device.device, vert_shader, nullptr);

	return OK;
}

Error Renderer::create_framebuffers() {

	_swapchain_images = _vkb_swapchain.get_images().value();
	_swapchain_image_views = _vkb_swapchain.get_image_views().value();

	_framebuffers.resize(_swapchain_image_views.size());

	VkResult err;

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

		err = vkCreateFramebuffer(_vkb_device.device,
				&framebuffer_info,
				nullptr,
				&_framebuffers[i]);

		ERR_FAIL_COND_V_MSG(
				err != VK_SUCCESS, FAIL, "Failed to create framebuffer[%d]", i);
	}

	return OK;
}

Error Renderer::create_command_pool() {

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex =
			_vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	VkResult err = vkCreateCommandPool(
			_vkb_device.device, &pool_info, nullptr, &_command_pool);
	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create command pool");

	return OK;
}

Error Renderer::create_vertex_buffer() {

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = sizeof(vertices[0]) * vertices.size();
	buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
								VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	// todo we should store multiple buffers and allocs in a vector
	// https://github.com/godotengine/godot/blob/92c04fa727e3fc507e31c1bce88beeceb98fb06a/drivers/vulkan/rendering_device_vulkan.cpp#L1373

	VkResult err = vmaCreateBuffer(_vma_allocator,
			&buffer_info,
			&alloc_info,
			&_vertex_buffer,
			&_vertex_buffer_alloc,
			nullptr);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to allocate vertex buffer: %d",
			(int)err);

	// update buffer
	// todo should be moved to an "update vertex buffers" type function

	void *data = nullptr;
	err = vmaMapMemory(_vma_allocator, _vertex_buffer_alloc, &data);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to map vertex buffer memory: %d",
			(int)err);

	memcpy(data, vertices.data(), (size_t)buffer_info.size);

	vmaUnmapMemory(_vma_allocator, _vertex_buffer_alloc);

	return OK;
}

Error Renderer::create_command_buffers() {

	_command_buffers.resize(_framebuffers.size());

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = _command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = (uint32_t)_command_buffers.size();

	VkResult err = vkAllocateCommandBuffers(
			_vkb_device.device, &alloc_info, _command_buffers.data());

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to allocate command buffers");

	for (size_t i = 0; i < _command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		err = vkBeginCommandBuffer(_command_buffers[i], &begin_info);

		ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
				FAIL,
				"Failed to begin command buffer [%d]",
				i);

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
		// render pass
		{
			vkCmdBindPipeline(_command_buffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					_graphics_pipeline);

			VkBuffer vertex_buffers[] = { _vertex_buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(
					_command_buffers[i], 0, 1, vertex_buffers, offsets);

			vkCmdDraw(_command_buffers[i],
					static_cast<uint32_t>(vertices.size()),
					1,
					0,
					0);
		}
		vkCmdEndRenderPass(_command_buffers[i]);

		err = vkEndCommandBuffer(_command_buffers[i]);

		ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
				FAIL,
				"Failed to end command buffer [%d]",
				i);
	}

	return OK;
}

Error Renderer::create_sync_objects() {

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
			return FAIL;
		}
	}

	return OK;
}

Error Renderer::recreate_swapchain() {
	vkDeviceWaitIdle(_vkb_device.device);

	vkDestroyCommandPool(_vkb_device.device, _command_pool, nullptr);
	for (auto framebuffer : _framebuffers) {
		vkDestroyFramebuffer(_vkb_device.device, framebuffer, nullptr);
	}
	_vkb_swapchain.destroy_image_views(_swapchain_image_views);

	ERR_FAIL_COND_V_MSG(create_swapchain(),
			FAIL,
			"Failed to create_swapchain when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_framebuffers(),
			FAIL,
			"Failed to create_framebuffers when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_command_pool(),
			FAIL,
			"Failed to create_command_pool when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_command_buffers(),
			FAIL,
			"Failed to create_command_buffers when recreating swapchain.");

	return OK;
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

	vmaDestroyBuffer(_vma_allocator, _vertex_buffer, _vertex_buffer_alloc);
	vmaDestroyAllocator(_vma_allocator);

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

		ERR_BREAK_MSG(draw_frame() != OK, "Failed to draw frame");
	}
}

Error Renderer::draw_frame() {

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
	} else {
		ERR_FAIL_COND_V_MSG(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR,
				FAIL,
				"Failed to acquire swapchain image: %s",
				result);
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

	result = vkQueueSubmit(_graphics_queue,
			1,
			&submit_info,
			_in_flight_fences[_current_frame]);

	ERR_FAIL_COND_V_MSG(result != VK_SUCCESS,
			FAIL,
			"Failed to submit draw command buffer: %s",
			result);

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
	} else {
		ERR_FAIL_COND_V_MSG(result != VK_SUCCESS,
				FAIL,
				"Failed to present swapchain image: %s",
				result);
	}

	_current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	return OK;
}
