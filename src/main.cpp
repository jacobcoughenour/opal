﻿

#include "opal.h"
#include <cstdlib>
#include <exception>
#include <iostream>

#include <imgui/imgui_impl_glfw.h>
#include <nvvk/context_vk.hpp>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


static void error_callback(int error, const char* description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {


	// create the window

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		return EXIT_FAILURE;
	}

	// window settings

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	const int WIDTH = 1280;
	const int HEIGHT = 720;

	auto* window = glfwCreateWindow(WIDTH, HEIGHT, "Opal", nullptr, nullptr);

	// check for vulkan support from glfw
	if (!glfwVulkanSupported()) {
		printf("GLFW: Vulkan support missing");
		return EXIT_FAILURE;
	}


	// Vulkan setup

	nvvk::ContextCreateInfo ctx_info(true);
	ctx_info.setVersion(1, 2);

	// INSTANCE LAYERS
	ctx_info.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);

	// INSTANCE EXTENSIONS
	ctx_info.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
	ctx_info.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef WIN32
	ctx_info.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	ctx_info.addInstanceExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	ctx_info.addInstanceExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

	// DEVICE EXTENSIONS
	ctx_info.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

	// get raytracing extension info
	vk::PhysicalDeviceRayTracingFeaturesKHR rt_features;
	ctx_info.addDeviceExtension(VK_KHR_RAY_TRACING_EXTENSION_NAME, false, &rt_features);


	// Vulkan app instance

	nvvk::Context ctx {};
	ctx.initInstance(ctx_info);

	// get list of devices with support for all our extensions
	auto compatible_devices = ctx.getCompatibleDevices(ctx_info);
	assert(!compatible_devices.empty());

	// use first compatible device
	// todo should we always pick the first one?
	ctx.initDevice(compatible_devices[0], ctx_info);


	// create the app

	Opal opal;

	// create a surface on the glfw window for vulkan to draw to
	const auto surface = opal.getVkSurface(ctx.m_instance, window);
	// feature check
	ctx.setGCTQueueWithPresent(surface);

	opal.setup(ctx.m_instance, ctx.m_device, ctx.m_physicalDevice,
			ctx.m_queueGCT.familyIndex);

	opal.createSurface(surface, WIDTH, HEIGHT);
	opal.createDepthBuffer();
	opal.createRenderPass();
	opal.createFrameBuffers();

	// initialize imgui
	opal.initGUI(0);



	// link app to the glfw window to receive event callbacks
	opal.setupGlfwCallbacks(window);

	// link imgui to glfw window
	ImGui_ImplGlfw_InitForVulkan(window, true);



	bool raytracing_enabled = true;
	nvmath::vec4f clear_color = nvmath::vec4f(1, 1, 1, 1.00f);

	vk::ClearValue clearValues[2];
	clearValues[0].setColor(std::array<float, 4>({
			clear_color[0], clear_color[1], clear_color[2], clear_color[3]
	}));
	clearValues[1].setDepthStencil({1.f, 0});



	// main event loop

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// skip when minimized
		if (opal.isMinimized())
			continue;

//		ImGui_ImplGlfw_NewFrame()


		// start new frame
		opal.prepareFrame();

		auto active_frame_index = opal.getCurFrame();
		const auto cmd_buf = opal.getCommandBuffers()[active_frame_index];

		cmd_buf.begin({
				vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		});

		{
//			vk::RenderPassBeginInfo offscreen_pass_info;
//			offscreen_pass_info.setClearValueCount(2);
//			offscreen_pass_info.setPClearValues(clearValues);
//			offscreen_pass_info.setRenderPass(opal.m_off)
//
//			if (raytracing_enabled) {
//				opal.raytrace(cmd_buf, clear_color);
//			} else {
//				cmd_buf.beginRenderPass(offscreen_pass_info, vk::SubpassContents::eInline);
//				opal.rasterize(cmd_buf);
//				cmd_buf.endRenderPass();
//			}

		}

		// commit frame
		cmd_buf.end();
		opal.submitFrame();
	}




	// exit

	// cleanup app
	opal.getDevice().waitIdle();
	opal.destroy();

	// cleanup vulkan
	ctx.deinit();

	// cleanup glfw window
	glfwDestroyWindow(window);
	glfwTerminate();


	return EXIT_SUCCESS;
}


