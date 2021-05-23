#include "renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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

	ERR_TRY(load_model());

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
			.require_api_version(
					VK_VERSION_MAJOR(VK_REQUIRED_API_VERSION),
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
	ERR_FAIL_COND_V_MSG(
			!instance_builder_return,
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
	_vk_surface	 = nullptr;
	VkResult err = glfwCreateWindowSurface(
			_vkb_instance.instance, _window, NULL, &_vk_surface);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			FAIL,
			"Failed to create glfw window surface: %s",
			err);

	return OK;
}

Error Renderer::create_vk_device() {

	// create device selector
	vkb::PhysicalDeviceSelector device_selector { _vkb_instance };

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

	vkb::DeviceBuilder device_builder { physical_device };
	auto device_builder_return = device_builder.build();

	ERR_FAIL_COND_V_MSG(
			!device_builder_return,
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

	VmaAllocatorCreateInfo allocator_info {
		.physicalDevice	  = _vkb_device.physical_device.physical_device,
		.device			  = _vkb_device.device,
		.instance		  = _vkb_instance.instance,
		.vulkanApiVersion = VK_REQUIRED_API_VERSION,
	};

	// get vulkan function pointers from volk

	VolkDeviceTable table;
	volkLoadDeviceTable(&table, _vkb_device.device);

	// remap volk table to vma table
	VmaVulkanFunctions vulkan_funcs {
		.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
		.vkGetPhysicalDeviceMemoryProperties =
				vkGetPhysicalDeviceMemoryProperties,
		.vkAllocateMemory = table.vkAllocateMemory,
		.vkFreeMemory = table.vkFreeMemory, .vkMapMemory = table.vkMapMemory,
		.vkUnmapMemory					= table.vkUnmapMemory,
		.vkFlushMappedMemoryRanges		= table.vkFlushMappedMemoryRanges,
		.vkInvalidateMappedMemoryRanges = table.vkInvalidateMappedMemoryRanges,
		.vkBindBufferMemory				= table.vkBindBufferMemory,
		.vkBindImageMemory				= table.vkBindImageMemory,
		.vkGetBufferMemoryRequirements	= table.vkGetBufferMemoryRequirements,
		.vkGetImageMemoryRequirements	= table.vkGetImageMemoryRequirements,
		.vkCreateBuffer					= table.vkCreateBuffer,
		.vkDestroyBuffer				= table.vkDestroyBuffer,
		.vkCreateImage					= table.vkCreateImage,
		.vkDestroyImage					= table.vkDestroyImage,
		.vkCmdCopyBuffer				= table.vkCmdCopyBuffer,
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
		.vkGetBufferMemoryRequirements2KHR =
				table.vkGetBufferMemoryRequirements2KHR,
		.vkGetImageMemoryRequirements2KHR =
				table.vkGetImageMemoryRequirements2KHR,
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
		.vkBindBufferMemory2KHR = table.vkBindBufferMemory2KHR,
		.vkBindImageMemory2KHR	= table.vkBindImageMemory2KHR,
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
		.vkGetPhysicalDeviceMemoryProperties2KHR =
				vkGetPhysicalDeviceMemoryProperties2KHR,
#endif
	};

	allocator_info.pVulkanFunctions = &vulkan_funcs;

	ERR_FAIL_COND_V_MSG(
			vmaCreateAllocator(&allocator_info, &_vma_allocator) != VK_SUCCESS,
			FAIL,
			"Failed to create Vulkan Memory Allocator");

	return OK;
}

Error Renderer::create_swapchain() {

	// (re)build swapchain
	vkb::SwapchainBuilder swapchain_builder { _vkb_device };
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

	_swapchain_images	   = _vkb_swapchain.get_images().value();
	_swapchain_image_views = _vkb_swapchain.get_image_views().value();

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		_swapchain_image_views[i] = create_image_view(
				_swapchain_images[i],
				_vkb_swapchain.image_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
	}

	return OK;
}

Error Renderer::get_queues() {

	// get graphics queue
	auto graphics_queue = _vkb_device.get_queue(vkb::QueueType::graphics);
	ERR_FAIL_COND_V_MSG(
			!graphics_queue.has_value(),
			FAIL,
			"Failed to get graphics queue: %s",
			graphics_queue.error().message().c_str());
	_graphics_queue = graphics_queue.value();

	// get presentation queue
	auto present_queue = _vkb_device.get_queue(vkb::QueueType::present);
	ERR_FAIL_COND_V_MSG(
			!present_queue.has_value(),
			FAIL,
			"Failed to get presentation queue: %s",
			present_queue.error().message().c_str());
	_present_queue = present_queue.value();

	return OK;
}

Error Renderer::create_render_pass() {

	VkAttachmentDescription color_attachment {
		.format			= _vkb_swapchain.image_format,
		.samples		= VK_SAMPLE_COUNT_1_BIT,
		.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp		= VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference color_attachment_ref {
		.attachment = 0,
		.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentDescription depth_attachment {
		.format			= find_depth_format(),
		.samples		= VK_SAMPLE_COUNT_1_BIT,
		.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentReference depth_attachment_ref {
		.attachment = 1,
		.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass {
		.pipelineBindPoint		 = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount	 = 1,
		.pColorAttachments		 = &color_attachment_ref,
		.pDepthStencilAttachment = &depth_attachment_ref,
	};

	VkSubpassDependency dependency {
		.srcSubpass	  = VK_SUBPASS_EXTERNAL,
		.dstSubpass	  = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
						VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
						VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
						 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
	};

	std::array<VkAttachmentDescription, 2> attachments {
		color_attachment,
		depth_attachment,
	};

	VkRenderPassCreateInfo pass_info {
		.sType			 = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments	 = attachments.data(),
		.subpassCount	 = 1,
		.pSubpasses		 = &subpass,
		.dependencyCount = 1,
		.pDependencies	 = &dependency,
	};

	// create render pass
	VkResult err = vkCreateRenderPass(
			_vkb_device.device, &pass_info, nullptr, &_render_pass);
	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create render pass");

	return OK;
}

Error Renderer::create_descriptor_set_layout() {

	VkDescriptorSetLayoutBinding ubo_layout_binding {
		.binding			= 0,
		.descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount	= 1,
		.stageFlags			= VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	};

	VkDescriptorSetLayoutBinding sampler_layout_binding {
		.binding		 = 1,
		.descriptorType	 = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		// only available for the fragment shader
		.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};

	std::array<VkDescriptorSetLayoutBinding, 2> bindings {
		ubo_layout_binding,
		sampler_layout_binding,
	};

	VkDescriptorSetLayoutCreateInfo layout_info {
		.sType		  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings	  = bindings.data(),
	};

	VkResult res = vkCreateDescriptorSetLayout(
			_vkb_device.device, &layout_info, nullptr, &_descriptor_set_layout);

	ERR_FAIL_COND_V_MSG(
			res != VK_SUCCESS,
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

	VkPipelineShaderStageCreateInfo vert_stage_info {
		.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage	= VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert_shader,
		.pName	= "main",
	};

	VkPipelineShaderStageCreateInfo frag_stage_info {
		.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage	= VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_shader,
		.pName	= "main",
	};

	VkPipelineShaderStageCreateInfo shader_stages[] {
		vert_stage_info,
		frag_stage_info,
	};

	auto binding_description	= Vertex::get_binding_description();
	auto attribute_descriptions = Vertex::get_attribute_descriptions();

	VkPipelineVertexInputStateCreateInfo vertex_input_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions	   = &binding_description,
		.vertexAttributeDescriptionCount =
				static_cast<uint32_t>(attribute_descriptions.size()),
		.pVertexAttributeDescriptions = attribute_descriptions.data(),
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly {
		.sType	  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkViewport viewport {
		.x		  = 0.0f,
		.y		  = 0.0f,
		.width	  = (float)_vkb_swapchain.extent.width,
		.height	  = (float)_vkb_swapchain.extent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D scissor {
		.offset = { 0, 0 },
		.extent = _vkb_swapchain.extent,
	};

	VkPipelineViewportStateCreateInfo viewport_state {
		.sType		   = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports	   = &viewport,
		.scissorCount  = 1,
		.pScissors	   = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable		 = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode			 = VK_POLYGON_MODE_FILL,
		.cullMode				 = VK_CULL_MODE_BACK_BIT,
		.frontFace				 = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable		 = VK_FALSE,
		.lineWidth				 = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisampling {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable  = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable	   = VK_TRUE,
		.depthWriteEnable	   = VK_TRUE,
		.depthCompareOp		   = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable	   = VK_FALSE,
		.front				   = {},
		.back				   = {},
		.minDepthBounds		   = 0.0f,
		.maxDepthBounds		   = 1.0f,
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment {
		.blendEnable	= VK_FALSE,
		.colorWriteMask = // rgba
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo color_blending {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable	 = VK_FALSE,
		.logicOp		 = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments	 = &color_blend_attachment,
		.blendConstants	 = { 0.0f, 0.0f, 0.0f, 0.0f },
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info {
		.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		//.pushConstantRangeCount = 0,
		.pSetLayouts = &_descriptor_set_layout,
	};

	VkResult err = vkCreatePipelineLayout(
			_vkb_device.device,
			&pipeline_layout_info,
			nullptr,
			&_pipeline_layout);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create pipeline layout");

	std::vector<VkDynamicState> dynamic_states {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
		.pDynamicStates	   = dynamic_states.data(),
	};

	VkGraphicsPipelineCreateInfo pipeline_info {
		.sType				 = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount			 = 2,
		.pStages			 = shader_stages,
		.pVertexInputState	 = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pViewportState		 = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState	 = &multisampling,
		.pDepthStencilState	 = &depth_stencil,
		.pColorBlendState	 = &color_blending,
		.pDynamicState		 = &dynamic_info,
		.layout				 = _pipeline_layout,
		.renderPass			 = _render_pass,
		.subpass			 = 0,
		.basePipelineHandle	 = VK_NULL_HANDLE,
	};

	err = vkCreateGraphicsPipelines(
			_vkb_device.device,
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

	_swapchain_images	   = _vkb_swapchain.get_images().value();
	_swapchain_image_views = _vkb_swapchain.get_image_views().value();

	_framebuffers.resize(_swapchain_image_views.size());

	VkResult err;

	for (size_t i = 0; i < _swapchain_image_views.size(); i++) {
		std::array<VkImageView, 2> attachments {
			_swapchain_image_views[i],
			_depth_image_view,
		};

		VkFramebufferCreateInfo framebuffer_info {
			.sType			 = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass		 = _render_pass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments	 = attachments.data(),
			.width			 = _vkb_swapchain.extent.width,
			.height			 = _vkb_swapchain.extent.height,
			.layers			 = 1,
		};

		err = vkCreateFramebuffer(
				_vkb_device.device,
				&framebuffer_info,
				nullptr,
				&_framebuffers[i]);

		ERR_FAIL_COND_V_MSG(
				err != VK_SUCCESS, FAIL, "Failed to create framebuffer[%d]", i);
	}

	return OK;
}

Error Renderer::create_command_pool() {

	VkCommandPoolCreateInfo pool_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex =
				_vkb_device.get_queue_index(vkb::QueueType::graphics).value(),
	};

	VkResult err = vkCreateCommandPool(
			_vkb_device.device, &pool_info, nullptr, &_command_pool);
	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create command pool");

	return OK;
}

Error Renderer::create_depth_resources() {

	VkFormat depth_format = find_depth_format();

	create_image(
			&_depth_image,
			_vkb_swapchain.extent.width,
			_vkb_swapchain.extent.height,
			depth_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	_depth_image_view = create_image_view(
			_depth_image.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

	transition_image_layout(
			&_depth_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return OK;
}

#ifdef USE_DEBUG_UTILS
void Renderer::_debug_object_name(
		VkObjectType type, uint64_t handle, const char *name) {

	VkDebugUtilsObjectNameInfoEXT name_info {
		.sType		  = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext		  = NULL,
		.objectType	  = type,
		.objectHandle = handle,
		.pObjectName  = name,
	};

	vkSetDebugUtilsObjectNameEXT(_vkb_device.device, &name_info);
}

void Renderer::_debug_begin_label(
		VkCommandBuffer command_buffer,
		const char *name,
		float r,
		float g,
		float b,
		float a) {

	VkDebugUtilsLabelEXT label_info {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= NULL,
		.pLabelName = name,
		.color		= { r, g, b, a },
	};

	vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Renderer::_debug_insert_label(
		VkCommandBuffer command_buffer,
		const char *name,
		float r,
		float g,
		float b,
		float a) {

	VkDebugUtilsLabelEXT label_info {
		.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext		= NULL,
		.pLabelName = name,
		.color		= { r, g, b, a },
	};

	vkCmdInsertDebugUtilsLabelEXT(command_buffer, &label_info);
}

void Renderer::_debug_end_label(VkCommandBuffer command_buffer) {
	vkCmdEndDebugUtilsLabelEXT(command_buffer);
}
#endif

VkCommandBuffer Renderer::_begin_single_use_command_buffer() {

	VkCommandBufferAllocateInfo alloc_info {
		.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool		= _command_pool,
		.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer command_buffer;
	VkResult res = vkAllocateCommandBuffers(
			_vkb_device.device, &alloc_info, &command_buffer);

	ERR_FAIL_COND_V_MSG(
			res != VK_SUCCESS,
			command_buffer,
			"Failed to allocate command buffer: %d",
			(int)res);

	VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	res = vkBeginCommandBuffer(command_buffer, &begin_info);

	ERR_FAIL_COND_V_MSG(
			res != VK_SUCCESS,
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

	VkSubmitInfo submit_info {
		.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers	= &command_buffer,
	};

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
	stbi_uc *pixels = stbi_load(
			TEXTURE_PATH.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	VkDeviceSize image_size = width * height * 4;

	ERR_FAIL_COND_V_MSG(!pixels, FAIL, "Failed to load image texture");

	// transfer the texture pixels to a staging buffer

	Buffer staging_buffer;
	create_buffer(
			&staging_buffer,
			image_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory while loading texture image: "
			"%d",
			(int)err);

	memcpy(data, pixels, static_cast<size_t>(image_size));
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	stbi_image_free(pixels);

	ERR_TRY(create_image(
			&_texture_image,
			width,
			height,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

	transition_image_layout(
			&_texture_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copy_buffer_to_image(&staging_buffer, &_texture_image);

	transition_image_layout(
			&_texture_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	destroy_and_free_buffer(&staging_buffer);

	return OK;
}

Error Renderer::create_texture_image_view() {
	_texture_image_view = create_image_view(
			_texture_image.image,
			_texture_image.format,
			VK_IMAGE_ASPECT_COLOR_BIT);
	return OK;
}

Error Renderer::create_texture_sampler() {

	VkSamplerCreateInfo sampler_info {
		.sType			  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter		  = VK_FILTER_LINEAR,
		.minFilter		  = VK_FILTER_LINEAR,
		.mipmapMode		  = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU	  = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV	  = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW	  = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias		  = 0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy	  = _vkb_device.physical_device.properties.limits
								 .maxSamplerAnisotropy,
		.compareEnable			 = VK_FALSE,
		.compareOp				 = VK_COMPARE_OP_ALWAYS,
		.minLod					 = 0.0f,
		.maxLod					 = 0.0f,
		.borderColor			 = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	VkResult err = vkCreateSampler(
			_vkb_device.device, &sampler_info, nullptr, &_texture_sampler);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to create sampler: %d", (int)err);

	return OK;
};

Error Renderer::create_image(
		Image *image,
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties) {

	VkImageCreateInfo image_info {
		.sType	   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags	   = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format	   = format,
		.extent {
				.width	= width,
				.height = height,
				.depth	= 1,
		},
		.mipLevels	   = 1,
		.arrayLayers   = 1,
		.samples	   = VK_SAMPLE_COUNT_1_BIT,
		.tiling		   = tiling,
		.usage		   = usage,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo alloc_info {
		.flags = 0,
		// todo do we need usage info here?
		.usage			= VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags	= 0,
		.preferredFlags = 0,
		.pool			= nullptr,
		// .pUserData = "nullptr".c_str(),
		// .memoryTypeBits = properties,
	};

	VkResult err = vmaCreateImage(
			_vma_allocator,
			&image_info,
			&alloc_info,
			&image->image,
			&image->alloc,
			nullptr);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to allocate image: %d", (int)err);

	image->extent	  = image_info.extent;
	image->format	  = format;
	image->tiling	  = tiling;
	image->usage	  = usage;
	image->properties = properties;

	return OK;
}

VkImageView Renderer::create_image_view(
		VkImage image, VkFormat format, VkImageAspectFlags aspect) {

	VkImageViewCreateInfo view_info {
		.sType	  = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image	  = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format	  = format,
		.subresourceRange {
				.aspectMask		= aspect,
				.baseMipLevel	= 0,
				.levelCount		= 1,
				.baseArrayLayer = 0,
				.layerCount		= 1,
		},
	};

	VkImageView image_view;
	VkResult err = vkCreateImageView(
			_vkb_device.device, &view_info, nullptr, &image_view);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			image_view,
			"Failed to create image view: %d",
			(int)err);

	return image_view;
}

Error Renderer::transition_image_layout(
		Image *image, VkImageLayout old_layout, VkImageLayout new_layout) {

	VkImageMemoryBarrier barrier {
		.sType				 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout			 = old_layout,
		.newLayout			 = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image				 = image->image,
		.subresourceRange {
				.baseMipLevel	= 0,
				.levelCount		= 1,
				.baseArrayLayer = 0,
				.layerCount		= 1,
		},
	};

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
	} else if (
			old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (
			old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
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

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(
			vkCmdPipelineBarrier,
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
	VkBufferImageCopy region {
		.bufferOffset	   = 0,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource {
				.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel		= 0,
				.baseArrayLayer = 0,
				.layerCount		= 1,
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = image->extent,
	};

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(
			vkCmdCopyBufferToImage,
			buffer->buffer,
			image->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			// btw multiple regions can be used if we need that in the future.
			&region);

	return OK;
}

Error Renderer::load_model() {

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	bool res = tinyobj::LoadObj(
			&attrib, &shapes, &materials, &err, MODEL_PATH.c_str());

	ERR_FAIL_COND_V_MSG(!res, FAIL, "Failed to load model: %s", err.c_str());

	std::unordered_map<Vertex, uint32_t> unique_vertices {};

	for (const auto &shape : shapes) {
		for (const auto &index : shape.mesh.indices) {
			Vertex vertex {
				.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				},
				.color = {
					1.0f, 1.0f, 1.0f
				},
				.tex_coord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
				},
			};

			if (unique_vertices.count(vertex) == 0) {
				unique_vertices[vertex] =
						static_cast<uint32_t>(_vertices.size());
				_vertices.push_back(vertex);
			}

			_indices.push_back(unique_vertices[vertex]);
		}
	}

	return OK;
}

Error Renderer::create_vertex_buffer() {

	uint32_t size = sizeof(_vertices[0]) * _vertices.size();

	Buffer staging_buffer;

	create_buffer(
			&staging_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data	 = nullptr;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory: %d",
			(int)err);

	memcpy(data, _vertices.data(), (size_t)size);
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	create_buffer(
			&_vertex_buffer,
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
	uint32_t size = sizeof(_indices[0]) * _indices.size();

	Buffer staging_buffer;

	create_buffer(
			&staging_buffer,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data	 = nullptr;
	VkResult err = vmaMapMemory(_vma_allocator, staging_buffer.alloc, &data);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			FAIL,
			"Failed to map staging buffer memory: %d",
			(int)err);

	memcpy(data, _indices.data(), (size_t)size);
	vmaUnmapMemory(_vma_allocator, staging_buffer.alloc);

	create_buffer(
			&_index_buffer,
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
				create_buffer(
						&_uniform_buffers[i],
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

	std::array<VkDescriptorPoolSize, 2> pool_sizes {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount =
			static_cast<uint32_t>(_swapchain_images.size());

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount =
			static_cast<uint32_t>(_swapchain_images.size());

	VkDescriptorPoolCreateInfo pool_info {
		.sType		   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets	   = static_cast<uint32_t>(_swapchain_images.size()),
		.poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
		.pPoolSizes	   = pool_sizes.data(),
	};

	VkResult res = vkCreateDescriptorPool(
			_vkb_device.device, &pool_info, nullptr, &_descriptor_pool);

	ERR_FAIL_COND_V_MSG(
			res != VK_SUCCESS,
			FAIL,
			"Failed to create descriptor pool: %d",
			(int)res);

	return OK;
}

Error Renderer::create_descriptor_sets() {

	std::vector<VkDescriptorSetLayout> layouts(
			_swapchain_images.size(), _descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info {
		.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool		= _descriptor_pool,
		.descriptorSetCount = static_cast<uint32_t>(_swapchain_images.size()),
		.pSetLayouts		= layouts.data(),
	};

	_descriptor_sets.resize(_swapchain_images.size());

	VkResult res = vkAllocateDescriptorSets(
			_vkb_device.device, &alloc_info, _descriptor_sets.data());

	ERR_FAIL_COND_V_MSG(
			res != VK_SUCCESS,
			FAIL,
			"Failed to allocate descriptor sets: %d",
			(int)res);

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		VkDescriptorBufferInfo buffer_info {
			.buffer = _uniform_buffers[i].buffer,
			.offset = 0,
			.range	= sizeof(UniformBufferObject),
		};

		VkDescriptorImageInfo image_info {
			.sampler	 = _texture_sampler,
			.imageView	 = _texture_image_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		std::array<VkWriteDescriptorSet, 2> descriptor_writes {};

		descriptor_writes[0].sType	= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].dstSet = _descriptor_sets[i];
		descriptor_writes[0].dstBinding		 = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].pBufferInfo	 = &buffer_info;

		descriptor_writes[1].sType	= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].dstSet = _descriptor_sets[i];
		descriptor_writes[1].dstBinding		 = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorType =
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].pImageInfo		 = &image_info;

		vkUpdateDescriptorSets(
				_vkb_device.device,
				static_cast<uint32_t>(descriptor_writes.size()),
				descriptor_writes.data(),
				0,
				nullptr);
	}

	return OK;
}

Error Renderer::update_uniform_buffer(uint32_t image_index) {

	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time	   = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(
						 current_time - start_time)
						 .count();

	UniformBufferObject ubo = {};
	ubo.model				= glm::rotate(
			  glm::mat4(1.0f),
			  time * glm::radians(90.0f),
			  glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(
			glm::vec3(2.0f, 2.0f, 2.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
			glm::radians(45.0f),
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

Error Renderer::create_buffer(
		Buffer *buffer,
		uint32_t size,
		uint32_t usage,
		VmaMemoryUsage mapping,
		VkMemoryPropertyFlags mem_flags) {

	VkBufferCreateInfo buffer_info {
		.sType		 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size		 = size,
		.usage		 = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo alloc_info {
		.usage			= mapping,
		.preferredFlags = mem_flags,
	};

	VkResult err = vmaCreateBuffer(
			_vma_allocator,
			&buffer_info,
			&alloc_info,
			&buffer->buffer,
			&buffer->alloc,
			nullptr);

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS,
			FAIL,
			"Failed to allocate vertex buffer: %d",
			(int)err);

	buffer->info.buffer = buffer->buffer;
	buffer->info.offset = 0;
	buffer->info.range	= size;
	buffer->size		= size;
	buffer->usage		= usage;

	return OK;
}

Error Renderer::copy_buffer(
		Buffer *src_buffer, Buffer *dst_buffer, uint32_t size) {

	// create a command buffer
	VkBufferCopy copy_region = {
		.size = size,
	};

	VK_SUBMIT_SINGLE_CMD_OR_FAIL(
			vkCmdCopyBuffer,
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
	buffer->alloc  = nullptr;
	buffer->size   = 0;

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

	VkCommandBufferAllocateInfo alloc_info {
		.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool		= _command_pool,
		.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (uint32_t)_command_buffers.size(),
	};

	VkResult err = vkAllocateCommandBuffers(
			_vkb_device.device, &alloc_info, _command_buffers.data());

	ERR_FAIL_COND_V_MSG(
			err != VK_SUCCESS, FAIL, "Failed to allocate command buffers");

	for (size_t i = 0; i < _command_buffers.size(); i++) {

		VK_DEBUG_OBJECT_NAME(
				VK_OBJECT_TYPE_COMMAND_BUFFER,
				(uint64_t)_command_buffers[i],
				("Command Buffer " + std::to_string(i)).c_str());

		VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};

		err = vkBeginCommandBuffer(_command_buffers[i], &begin_info);

		ERR_FAIL_COND_V_MSG(
				err != VK_SUCCESS,
				FAIL,
				"Failed to begin command buffer [%d]",
				i);

		VkRenderPassBeginInfo render_pass_info {
			.sType		 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass	 = _render_pass,
			.framebuffer = _framebuffers[i],
			.renderArea {
					.offset = { 0, 0 },
					.extent = _vkb_swapchain.extent,
			},
		};

		std::array<VkClearValue, 2> clear_colors {};
		clear_colors[0].color		 = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_colors[1].depthStencil = { 1.0f, 0 };

		render_pass_info.clearValueCount =
				static_cast<uint32_t>(clear_colors.size());
		render_pass_info.pClearValues = clear_colors.data();

		VkViewport viewport {
			.x		  = 0.0f,
			.y		  = 0.0f,
			.width	  = (float)_vkb_swapchain.extent.width,
			.height	  = (float)_vkb_swapchain.extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		VkRect2D scissor = {
			.offset = { 0, 0 },
			.extent = _vkb_swapchain.extent,
		};

		VK_DEBUG_BEGIN_LABEL(
				_command_buffers[i], "render pass", 0.0f, 0.0f, 1.0f, 1.0f);

		vkCmdSetViewport(_command_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(_command_buffers[i], 0, 1, &scissor);
		vkCmdBeginRenderPass(
				_command_buffers[i],
				&render_pass_info,
				VK_SUBPASS_CONTENTS_INLINE);
		// render pass
		{
			vkCmdBindPipeline(
					_command_buffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					_graphics_pipeline);

			VkBuffer vertex_buffers[] = { _vertex_buffer.buffer };
			VkDeviceSize offsets[]	  = { 0 };
			vkCmdBindVertexBuffers(
					_command_buffers[i], 0, 1, vertex_buffers, offsets);

			vkCmdBindIndexBuffer(
					_command_buffers[i],
					_index_buffer.buffer,
					0,
					VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(
					_command_buffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					_pipeline_layout,
					0,
					1,
					&_descriptor_sets[i],
					0,
					nullptr);

			VK_DEBUG_INSERT_LABEL(
					_command_buffers[i],
					"draw indexed",
					1.0f,
					0.0f,
					1.0f,
					1.0f);

			vkCmdDrawIndexed(
					_command_buffers[i],
					static_cast<uint32_t>(_indices.size()),
					1,
					0,
					0,
					0);
		}
		vkCmdEndRenderPass(_command_buffers[i]);

		VK_DEBUG_END_LABEL(_command_buffers[i]);

		err = vkEndCommandBuffer(_command_buffers[i]);

		ERR_FAIL_COND_V_MSG(
				err != VK_SUCCESS,
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

	VkSemaphoreCreateInfo semaphore_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VkFenceCreateInfo fence_info {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(
					_vkb_device.device,
					&semaphore_info,
					nullptr,
					&_available_semaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(
					_vkb_device.device,
					&semaphore_info,
					nullptr,
					&_finished_semaphores[i]) != VK_SUCCESS ||
			vkCreateFence(
					_vkb_device.device,
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
		} else if (
				tiling == VK_IMAGE_TILING_OPTIMAL &&
				(props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	LOG_ERR("Failed to find supported format");
	return VK_FORMAT_UNDEFINED;
}

VkFormat Renderer::find_depth_format() {
	return find_supported_format(
			{ VK_FORMAT_D32_SFLOAT,
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

	int width  = 0;
	int height = 0;
	glfwGetFramebufferSize(_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(_vkb_device.device);

	destroy_swapchain();

	ERR_FAIL_COND_V_MSG(
			create_swapchain(),
			FAIL,
			"Failed to create_swapchain when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_image_views(),
			FAIL,
			"Failed to create_image_views when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_render_pass(),
			FAIL,
			"Failed to create_render_pass when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_graphics_pipeline(),
			FAIL,
			"Failed to create_graphics_pipeline when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_depth_resources(),
			FAIL,
			"Failed to create_depth_resources when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_framebuffers(),
			FAIL,
			"Failed to create_framebuffers when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_uniform_buffers(),
			FAIL,
			"Failed to create_uniform_buffers when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_descriptor_pool(),
			FAIL,
			"Failed to create_descriptor_pool when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_descriptor_sets(),
			FAIL,
			"Failed to create_descriptor_sets when recreating swapchain.");
	ERR_FAIL_COND_V_MSG(
			create_command_buffers(),
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

	vkFreeCommandBuffers(
			_vkb_device.device,
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

	vkWaitForFences(
			_vkb_device.device,
			1,
			&_in_flight_fences[_current_frame],
			VK_TRUE,
			UINT64_MAX);

	uint32_t image_index = 0;

	VkResult result = vkAcquireNextImageKHR(
			_vkb_device.device,
			_vkb_swapchain.swapchain,
			UINT64_MAX,
			_available_semaphores[_current_frame],
			VK_NULL_HANDLE,
			&image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreate_swapchain();
	} else {
		ERR_FAIL_COND_V_MSG(
				result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR,
				FAIL,
				"Failed to acquire swapchain image: %s",
				result);
	}

	update_uniform_buffer(image_index);

	if (_images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(
				_vkb_device.device,
				1,
				&_images_in_flight[image_index],
				VK_TRUE,
				UINT64_MAX);
	}

	_images_in_flight[image_index] = _in_flight_fences[_current_frame];

	VkSemaphore wait_semaphores[] {
		_available_semaphores[_current_frame],
	};
	VkPipelineStageFlags wait_stages[] {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	};
	VkSemaphore signal_semaphores[] {
		_finished_semaphores[_current_frame],
	};

	VkSubmitInfo submit_info {
		.sType				  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount	  = 1,
		.pWaitSemaphores	  = wait_semaphores,
		.pWaitDstStageMask	  = wait_stages,
		.commandBufferCount	  = 1,
		.pCommandBuffers	  = &_command_buffers[image_index],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores	  = signal_semaphores,
	};

	vkResetFences(_vkb_device.device, 1, &_in_flight_fences[_current_frame]);

	result = vkQueueSubmit(
			_graphics_queue,
			1,
			&submit_info,
			_in_flight_fences[_current_frame]);

	ERR_FAIL_COND_V_MSG(
			result != VK_SUCCESS,
			FAIL,
			"Failed to submit draw command buffer: %s",
			result);

	VkSwapchainKHR swapchains[] { _vkb_swapchain.swapchain };

	VkPresentInfoKHR present_info {
		.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores	= signal_semaphores,
		.swapchainCount		= 1,
		.pSwapchains		= swapchains,
		.pImageIndices		= &image_index,
	};

	result = vkQueuePresentKHR(_present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return recreate_swapchain();
	} else {
		ERR_FAIL_COND_V_MSG(
				result != VK_SUCCESS,
				FAIL,
				"Failed to present swapchain image: %s",
				result);
	}

	_current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	return OK;
}
