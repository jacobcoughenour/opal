#include "renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace Opal;

static void glfw_error_callback(int error, const char *description) {
	LOG_ERR("GLFW Error %d: %s", error, description);
}

Error Renderer::initialize() {

	ERR_FAIL_COND_V_MSG(volkInitialize(), FAIL, "Failed to initialize Volk");

	ERR_TRY(create_window());
	ERR_TRY(create_vk_instance());
	ERR_TRY(create_surface());
	ERR_TRY(create_vk_device());
	ERR_TRY(create_vma_allocator());
	ERR_TRY(create_swapchain());
	ERR_TRY(create_image_views());
	ERR_TRY(get_queues());
	ERR_TRY(create_render_pass());
	ERR_TRY(create_descriptor_set_layout());
	ERR_TRY(create_graphics_pipeline());
	ERR_TRY(create_command_pool());
	ERR_TRY(create_depth_resources());
	ERR_TRY(create_framebuffers());

	ERR_TRY(create_texture_image());
	ERR_TRY(create_texture_image_view());
	ERR_TRY(create_texture_sampler());

	ERR_TRY(create_vertex_buffer());
	ERR_TRY(create_index_buffer());
	ERR_TRY(create_uniform_buffers());
	ERR_TRY(create_descriptor_pool());
	ERR_TRY(create_descriptor_sets());
	ERR_TRY(create_command_buffers());
	ERR_TRY(create_sync_objects());

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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

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
				LOG("VK [%s: %s]\n\t%s\n",
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
			instance_builder_return.error().message().c_str());

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

	// add device features
	device_selector.set_required_features(VK_REQUIRED_DEVICE_FEATURES);
	device_selector.set_required_features_11(VK_REQUIRED_DEVICE_FEATURES_11);
	device_selector.set_required_features_12(VK_REQUIRED_DEVICE_FEATURES_12);

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
			device_builder_return.error().message().c_str());

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
							// swapchain settings
							.build();

	if (!swap_ret) {
		LOG_ERR("Failed to create Vulkan swapchain: %s",
				swap_ret.error().message().c_str());
		_vkb_swapchain.swapchain = VK_NULL_HANDLE;
		return FAIL;
	}

	// get final swapchain
	_vkb_swapchain = swap_ret.value();

	return OK;
}

Error Renderer::create_image_views() {

	_swapchain_image_views.resize(_swapchain_images.size());

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		_swapchain_image_views[i] = create_image_view(_swapchain_images[i],
				_vkb_swapchain.image_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
	}

	return OK;
}

Error Renderer::get_queues() {

	// get graphics queue
	auto graphics_queue = _vkb_device.get_queue(vkb::QueueType::graphics);
	ERR_FAIL_COND_V_MSG(!graphics_queue.has_value(),
			FAIL,
			"Failed to get graphics queue: %s",
			graphics_queue.error().message().c_str());
	_graphics_queue = graphics_queue.value();

	// get presentation queue
	auto present_queue = _vkb_device.get_queue(vkb::QueueType::present);
	ERR_FAIL_COND_V_MSG(!present_queue.has_value(),
			FAIL,
			"Failed to get presentation queue: %s",
			present_queue.error().message().c_str());
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

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format			= find_depth_format();
	depth_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout	= 
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment	= 1;
	depth_attachment_ref.layout		= 
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_attachment_ref;
	subpass.pDepthStencilAttachment	= &depth_attachment_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
								  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
								  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
								 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = {
		color_attachment,
		depth_attachment
	};

	VkRenderPassCreateInfo pass_info = {};
	pass_info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.attachmentCount	= static_cast<uint32_t>(attachments.size());
	pass_info.pAttachments		= attachments.data();
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

Error Renderer::create_descriptor_set_layout() {

	VkDescriptorSetLayoutBinding ubo_layout_binding = {};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.pImmutableSamplers = nullptr;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding sampler_layout_binding = {};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = nullptr;
	// only available for the fragment shader
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding,
		sampler_layout_binding };

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings = bindings.data();

	VkResult res = vkCreateDescriptorSetLayout(
			_vkb_device.device, &layout_info, nullptr, &_descriptor_set_layout);

	ERR_FAIL_COND_V_MSG(res != VK_SUCCESS,
			FAIL,
			"Failed to create descriptor set layout: %d",
			(int)res);

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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	depth_stencil.front = {};
	depth_stencil.back = {};

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
	pipeline_layout_info.setLayoutCount = 1;
	// pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pSetLayouts = &_descriptor_set_layout;

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
	pipeline_info.pDepthStencilState = &depth_stencil;
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
		std::array<VkImageView, 2> attachments = { _swapchain_image_views[i],
			_depth_image_view };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = _render_pass;
		framebuffer_info.attachmentCount =
				static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
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

Error Renderer::create_depth_resources() {

	VkFormat depth_format = find_depth_format();

	create_image(&_depth_image,
			_vkb_swapchain.extent.width,
			_vkb_swapchain.extent.height,
			depth_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	_depth_image_view = create_image_view(
			_depth_image.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

	transition_image_layout(&_depth_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return OK;
}

#ifdef USE_DEBUG_UTILS
void Renderer::_debug_object_name(
		VkObjectType type, uint64_t handle, const char *name) {

	VkDebugUtilsObjectNameInfoEXT name_info = {};
	name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	name_info.pNext = NULL;
	name_info.objectType = type;
	name_info.objectHandle = handle;
	name_info.pObjectName = name;

	vkSetDebugUtilsObjectNameEXT(_vkb_device.device, &name_info);
}

void Renderer::_debug_begin_label(VkCommandBuffer command_buffer,
		const char *name,
		float r,
		float g,
		float b,
		float a) {

	VkDebugUtilsLabelEXT label_info = {};
	label_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label_info.pNext = NULL;
	label_info.pLabelName = name;
	label_info.color[0] = r;
	label_info.color[1] = g;
	label_info.color[2] = b;
	label_info.color[3] = a;

	vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Renderer::_debug_insert_label(VkCommandBuffer command_buffer,
		const char *name,
		float r,
		float g,
		float b,
		float a) {

	VkDebugUtilsLabelEXT label_info = {};
	label_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label_info.pNext = NULL;
	label_info.pLabelName = name;
	label_info.color[0] = r;
	label_info.color[1] = g;
	label_info.color[2] = b;
	label_info.color[3] = a;

	vkCmdInsertDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Renderer::_debug_end_label(VkCommandBuffer command_buffer) {
	vkCmdEndDebugUtilsLabelEXT(command_buffer);
}
#endif

VkCommandBuffer Renderer::_begin_single_use_command_buffer() {

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = _command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VkResult res = vkAllocateCommandBuffers(
			_vkb_device.device, &alloc_info, &command_buffer);

	ERR_FAIL_COND_V_MSG(res != VK_SUCCESS,
			command_buffer,
			"Failed to allocate command buffer: %d",
			(int)res);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	res = vkBeginCommandBuffer(command_buffer, &begin_info);

	ERR_FAIL_COND_V_MSG(res != VK_SUCCESS,
			command_buffer,
			"Failed to begin command buffer: %d",
			(int)res);

	return command_buffer;
}

void Renderer::_end_and_submit_single_use_command_buffer(
		VkCommandBuffer command_buffer) {

	VkResult res = vkEndCommandBuffer(command_buffer);

	ERR_FAIL_COND_MSG(
			res != VK_SUCCESS, "Failed to end command buffer: %d", (int)res);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	res = vkQueueSubmit(_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	ERR_FAIL_COND_MSG(
			res != VK_SUCCESS, "Failed to submit queue: %d", (int)res);

	res = vkQueueWaitIdle(_graphics_queue);
	ERR_FAIL_COND_MSG(
			res != VK_SUCCESS, "Failed to wait for queue: %d", (int)res);

	vkFreeCommandBuffers(_vkb_device.device, _command_pool, 1, &command_buffer);
}

Error Renderer::create_texture_image() {

	// todo move this to a util class or something

	int width, height, channels;
	stbi_uc *pixels = stbi_load("assets/textures/texture.jpg",
			&width,
			&height,
			&channels,
			STBI_rgb_alpha);

	VkDeviceSize image_size = width * height * 4;

	ERR_FAIL_COND_V_MSG(!pixels, FAIL, "Failed to load image texture");

	// transfer the texture pixels to a staging buffer

	Buffer staging_buffer;
	create_buffer(&staging_buffer,
			image_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory while loading texture image: "
			"%d",
			(int)err);

	memcpy(data, pixels, static_cast<size_t>(image_size));
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	stbi_image_free(pixels);

	ERR_TRY(create_image(&_texture_image,
			width,
			height,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

	transition_image_layout(&_texture_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copy_buffer_to_image(&staging_buffer, &_texture_image);

	transition_image_layout(&_texture_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	destroy_and_free_buffer(&staging_buffer);

	return OK;
}

Error Renderer::create_texture_image_view() {
	_texture_image_view = create_image_view(_texture_image.image,
			_texture_image.format,
			VK_IMAGE_ASPECT_COLOR_BIT);
	return OK;
}

Error Renderer::create_texture_sampler() {

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy =
			_vkb_device.physical_device.properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	VkResult err = vkCreateSampler(
			_vkb_device.device, &sampler_info, nullptr, &_texture_sampler);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create sampler: %d", (int)err);

	return OK;
};

Error Renderer::create_image(Image *image,
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties) {

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.requiredFlags = 0;
	alloc_info.preferredFlags = 0;
	alloc_info.flags = 0;
	alloc_info.pool = nullptr;
	// alloc_info.pUserData = "nullptr".c_str();
	// alloc_info.memoryTypeBits = properties;
	// todo do we need usage info here?
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkResult err = vmaCreateImage(_vma_allocator,
			&image_info,
			&alloc_info,
			&image->image,
			&image->alloc,
			nullptr);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to allocate image: %d", (int)err);

	image->extent = image_info.extent;
	image->format = format;
	image->tiling = tiling;
	image->usage = usage;
	image->properties = properties;

	return OK;
}

VkImageView Renderer::create_image_view(
		VkImage image, VkFormat format, VkImageAspectFlags aspect) {

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.format = format;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.subresourceRange.aspectMask = aspect;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	VkResult err = vkCreateImageView(
			_vkb_device.device, &view_info, nullptr, &image_view);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			image_view,
			"Failed to create image view: %d",
			(int)err);

	return image_view;
}

Error Renderer::transition_image_layout(
		Image *image, VkImageLayout old_layout, VkImageLayout new_layout) {

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (has_stencil_component(image->format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	} else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;

	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
			new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			   new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
			   new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
								VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else {
		LOG_ERR("Unsupported layout transition");
		return FAIL;
	}

	// access table
	// https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(vkCmdPipelineBarrier,
			src_stage,
			dst_stage,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&barrier);

	return OK;
}

Error Renderer::copy_buffer_to_image(Buffer *buffer, Image *image) {

	// define the region we are copying
	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = image->extent;

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(vkCmdCopyBufferToImage,
			buffer->buffer,
			image->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			// btw multiple regions can be used if we need that in the future.
			&region);

	return OK;
}

Error Renderer::create_vertex_buffer() {

	uint32_t size = sizeof(vertices[0]) * vertices.size();

	Buffer staging_buffer;

	create_buffer(&staging_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data = nullptr;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory: %d",
			(int)err);

	memcpy(data, vertices.data(), (size_t)size);
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	create_buffer(&_vertex_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	copy_buffer(&staging_buffer, &_vertex_buffer, size);

	destroy_and_free_buffer(&staging_buffer);

	return OK;
}

Error Renderer::create_index_buffer() {
	uint32_t size = sizeof(indices[0]) * indices.size();

	Buffer staging_buffer;

	create_buffer(&staging_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data = nullptr;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory: %d",
			(int)err);

	memcpy(data, indices.data(), (size_t)size);
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	create_buffer(&_index_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	copy_buffer(&staging_buffer, &_index_buffer, size);

	destroy_and_free_buffer(&staging_buffer);

	return OK;
}

Error Renderer::create_uniform_buffers() {

	VkDeviceSize size = sizeof(UniformBufferObject);

	_uniform_buffers.resize(_swapchain_images.size());

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		ERR_FAIL_COND_V_MSG(
				create_buffer(&_uniform_buffers[i],
						size,
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						VMA_MEMORY_USAGE_GPU_ONLY,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
								VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != OK,
				FAIL,
				"Failed to create uniform buffer %d",
				i);
	}

	return OK;
}

Error Renderer::create_descriptor_pool() {

	std::array<VkDescriptorPoolSize, 2> pool_sizes{};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount =
			static_cast<uint32_t>(_swapchain_images.size());

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount =
			static_cast<uint32_t>(_swapchain_images.size());

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = static_cast<uint32_t>(_swapchain_images.size());

	VkResult res = vkCreateDescriptorPool(
			_vkb_device.device, &pool_info, nullptr, &_descriptor_pool);

	ERR_FAIL_COND_V_MSG(res != VK_SUCCESS,
			FAIL,
			"Failed to create descriptor pool: %d",
			(int)res);

	return OK;
}

Error Renderer::create_descriptor_sets() {

	std::vector<VkDescriptorSetLayout> layouts(
			_swapchain_images.size(), _descriptor_set_layout);
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount =
			static_cast<uint32_t>(_swapchain_images.size());
	alloc_info.pSetLayouts = layouts.data();

	_descriptor_sets.resize(_swapchain_images.size());

	VkResult res = vkAllocateDescriptorSets(
			_vkb_device.device, &alloc_info, _descriptor_sets.data());

	ERR_FAIL_COND_V_MSG(res != VK_SUCCESS,
			FAIL,
			"Failed to allocate descriptor sets: %d",
			(int)res);

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = _uniform_buffers[i].buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = _texture_image_view;
		image_info.sampler = _texture_sampler;

		std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].dstSet = _descriptor_sets[i];
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].pBufferInfo = &buffer_info;

		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].dstSet = _descriptor_sets[i];
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorType =
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].pImageInfo = &image_info;

		vkUpdateDescriptorSets(_vkb_device.device,
				static_cast<uint32_t>(descriptor_writes.size()),
				descriptor_writes.data(),
				0,
				nullptr);
	}

	return OK;
}

Error Renderer::update_uniform_buffer(uint32_t image_index) {

	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(
			current_time - start_time)
						 .count();

	UniformBufferObject ubo = {};
	ubo.model = glm::rotate(glm::mat4(1.0f),
			time * glm::radians(90.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f),
			_vkb_swapchain.extent.width / (float)_vkb_swapchain.extent.height,
			0.1f,
			10.0f);
	ubo.proj[1][1] *= -1;

	void *data;
	vmaMapMemory(_vma_allocator, _uniform_buffers[image_index].alloc, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vmaUnmapMemory(_vma_allocator, _uniform_buffers[image_index].alloc);

	return OK;
}

Error Renderer::create_buffer(Buffer *buffer,
		uint32_t size,
		uint32_t usage,
		VmaMemoryUsage mapping,
		VkMemoryPropertyFlags mem_flags) {

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = mapping;
	alloc_info.preferredFlags = mem_flags;

	VkResult err = vmaCreateBuffer(_vma_allocator,
			&buffer_info,
			&alloc_info,
			&buffer->buffer,
			&buffer->alloc,
			nullptr);

	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS,
			FAIL,
			"Failed to allocate vertex buffer: %d",
			(int)err);

	buffer->info.buffer = buffer->buffer;
	buffer->info.offset = 0;
	buffer->info.range = size;
	buffer->size = size;
	buffer->usage = usage;

	return OK;
}

Error Renderer::copy_buffer(
		Buffer *src_buffer, Buffer *dst_buffer, uint32_t size) {

	// create a command buffer
	VkBufferCopy copy_region = {};
	copy_region.size = size;

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(vkCmdCopyBuffer,
			src_buffer->buffer,
			dst_buffer->buffer,
			1,
			&copy_region);

	return OK;
}

Error Renderer::destroy_and_free_buffer(Buffer *buffer) {

	vmaDestroyBuffer(_vma_allocator, buffer->buffer, buffer->alloc);

	// clean up the Buffer struct
	buffer->buffer = VK_NULL_HANDLE;
	buffer->alloc = nullptr;
	buffer->size = 0;

	return OK;
}

Error Renderer::destroy_and_free_image(Image *image) {

	vmaDestroyImage(_vma_allocator, image->image, image->alloc);

	// clean up the image struct
	image->image = VK_NULL_HANDLE;
	image->alloc = nullptr;

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

		VK_DEBUG_OBJECT_NAME(VK_OBJECT_TYPE_COMMAND_BUFFER,
				(uint64_t)_command_buffers[i],
				("Command Buffer " + std::to_string(i)).c_str());

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

		std::array<VkClearValue, 2> clear_colors = {};
		clear_colors[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_colors[1].depthStencil = { 1.0f, 0 };

		render_pass_info.clearValueCount =
				static_cast<uint32_t>(clear_colors.size());
		render_pass_info.pClearValues = clear_colors.data();

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

		VK_DEBUG_BEGIN_LABEL(
				_command_buffers[i], "render pass", 0.0f, 0.0f, 1.0f, 1.0f);

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

			VkBuffer vertex_buffers[] = { _vertex_buffer.buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(
					_command_buffers[i], 0, 1, vertex_buffers, offsets);

			vkCmdBindIndexBuffer(_command_buffers[i],
					_index_buffer.buffer,
					0,
					VK_INDEX_TYPE_UINT16);

			vkCmdBindDescriptorSets(_command_buffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					_pipeline_layout,
					0,
					1,
					&_descriptor_sets[i],
					0,
					nullptr);

			VK_DEBUG_INSERT_LABEL(_command_buffers[i],
					"draw indexed",
					1.0f,
					0.0f,
					1.0f,
					1.0f);

			vkCmdDrawIndexed(_command_buffers[i],
					static_cast<uint32_t>(indices.size()),
					1,
					0,
					0,
					0);
		}
		vkCmdEndRenderPass(_command_buffers[i]);

		VK_DEBUG_END_LABEL(_command_buffers[i]);

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
	_images_in_flight.resize(_vkb_swapchain.image_count, VK_NULL_HANDLE);

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

VkFormat Renderer::find_supported_format(
		const std::vector<VkFormat> &candidates,
		VkImageTiling tiling,
		VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(
				_vkb_device.physical_device.physical_device, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR &&
				(props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
				   (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	LOG_ERR("Failed to find supported format");
	return VK_FORMAT_UNDEFINED;
}

VkFormat Renderer::find_depth_format() {
	return find_supported_format({ VK_FORMAT_D32_SFLOAT,
										 VK_FORMAT_D32_SFLOAT_S8_UINT,
										 VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool Renderer::has_stencil_component(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
		   format == VK_FORMAT_D24_UNORM_S8_UINT;
}

Error Renderer::recreate_swapchain() {

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(_vkb_device.device);

	destroy_swapchain();

	ERR_FAIL_COND_V_MSG(create_swapchain(),
			FAIL,
			"Failed to create_swapchain when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_image_views(),
			FAIL,
			"Failed to create_image_views when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_render_pass(),
			FAIL,
			"Failed to create_render_pass when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_graphics_pipeline(),
			FAIL,
			"Failed to create_graphics_pipeline when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_depth_resources(),
			FAIL,
			"Failed to create_depth_resources when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_framebuffers(),
			FAIL,
			"Failed to create_framebuffers when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_uniform_buffers(),
			FAIL,
			"Failed to create_uniform_buffers when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_descriptor_pool(),
			FAIL,
			"Failed to create_descriptor_pool when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_descriptor_sets(),
			FAIL,
			"Failed to create_descriptor_sets when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(create_command_buffers(),
			FAIL,
			"Failed to create_command_buffers when recreating swapchain.");

	_images_in_flight.resize(_swapchain_images.size(), VK_NULL_HANDLE);

	return OK;
}

Error Renderer::destroy_swapchain() {

	vkDestroyImageView(_vkb_device.device, _depth_image_view, nullptr);
	destroy_and_free_image(&_depth_image);

	for (auto framebuffer : _framebuffers) {
		vkDestroyFramebuffer(_vkb_device.device, framebuffer, nullptr);
	}

	vkFreeCommandBuffers(_vkb_device.device,
			_command_pool,
			static_cast<uint32_t>(_command_buffers.size()),
			_command_buffers.data());

	vkDestroyPipeline(_vkb_device.device, _graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(_vkb_device.device, _pipeline_layout, nullptr);
	vkDestroyRenderPass(_vkb_device.device, _render_pass, nullptr);

	_vkb_swapchain.destroy_image_views(_swapchain_image_views);

	vkb::destroy_swapchain(_vkb_swapchain);

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		destroy_and_free_buffer(&_uniform_buffers[i]);
	}

	vkDestroyDescriptorPool(_vkb_device.device, _descriptor_pool, nullptr);

	return OK;
}

void Renderer::destroy() {

	// cleanup vulkan

	vkDeviceWaitIdle(_vkb_device.device);

	destroy_swapchain();

	vkDestroySampler(_vkb_device.device, _texture_sampler, nullptr);
	vkDestroyImageView(_vkb_device.device, _texture_image_view, nullptr);

	destroy_and_free_image(&_texture_image);

	vkDestroyDescriptorSetLayout(
			_vkb_device.device, _descriptor_set_layout, nullptr);

	destroy_and_free_buffer(&_vertex_buffer);
	destroy_and_free_buffer(&_index_buffer);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(
				_vkb_device.device, _finished_semaphores[i], nullptr);
		vkDestroySemaphore(
				_vkb_device.device, _available_semaphores[i], nullptr);
		vkDestroyFence(_vkb_device.device, _in_flight_fences[i], nullptr);
	}

	vkDestroyCommandPool(_vkb_device.device, _command_pool, nullptr);

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

	vkDeviceWaitIdle(_vkb_device.device);
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

	update_uniform_buffer(image_index);

	if (_images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(_vkb_device.device,
				1,
				&_images_in_flight[image_index],
				VK_TRUE,
				UINT64_MAX);
	}

	_images_in_flight[image_index] = _in_flight_fences[_current_frame];

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
