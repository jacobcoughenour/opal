#include "opal.h"
#include <cstdlib>
#include <exception>
#include <iostream>

#include <imgui/imgui_impl_glfw.h>
#include <nvpsystem.hpp>
#include <nvvk/context_vk.hpp>
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// Default search path for shaders
std::vector<std::string> default_search_paths;

static void error_callback(int error, const char *description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char **argv) {
	// create the window

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		return EXIT_FAILURE;
	}

	// window settings

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	const int WIDTH = 1280;
	const int HEIGHT = 720;

	auto *window = glfwCreateWindow(WIDTH, HEIGHT, PROJECT_NAME, nullptr, nullptr);

	// check for vulkan support from glfw
	if (!glfwVulkanSupported()) {
		printf("GLFW: Vulkan support missing");
		return EXIT_FAILURE;
	}

	NVPSystem system(argv[0], PROJECT_NAME);

	// add search paths
	default_search_paths = { NVPSystem::exePath(), PROJECT_ABSDIRECTORY, PROJECT_ABSDIRECTORY "../", PROJECT_NAME };

	// Vulkan setup

	nvvk::ContextCreateInfo ctx_info(true);
	ctx_info.setVersion(1, 2);

	// INSTANCE LAYERS
	ctx_info.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);

	// INSTANCE EXTENSIONS
	ctx_info.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
	ctx_info.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef WIN32
	ctx_info.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	ctx_info.addInstanceExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	ctx_info.addInstanceExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
	ctx_info.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// DEVICE EXTENSIONS
	ctx_info.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

	// ray tracing specific extensions
	ctx_info.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	ctx_info.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

	// activate ray tracing extensions and get device props when they are found
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR accel_features;
	ctx_info.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accel_features);
	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features;
	ctx_info.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rt_pipeline_features);

	// Vulkan app instance

	nvvk::Context ctx{};
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

	opal.setup(ctx.m_instance, ctx.m_device, ctx.m_physicalDevice, ctx.m_queueGCT.familyIndex, surface, WIDTH, HEIGHT);

	// link app to the glfw window to receive event callbacks
	opal.setupGlfwCallbacks(window);

	// link imgui to glfw window
	ImGui_ImplGlfw_InitForVulkan(window, true);

	// main event loop

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// skip when minimized
		if (opal.isMinimized())
			continue;

		opal.render();
	}

	// exit

	// cleanup app
	opal.getDevice().waitIdle();
	opal.destroyResources();
	opal.destroy();

	// cleanup vulkan
	ctx.deinit();

	// cleanup glfw window
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
