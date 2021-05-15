#include "renderer.h"

using namespace Opal;

static void glfw_error_callback(int error, const char *description) {
	LOG_ERR("GLFW Error %d: %s", error, description);
}

int Renderer::initialize() {

	// set callback for logging glfw errors
	glfwSetErrorCallback(glfw_error_callback);

	// initialize glfw library
	if (!glfwInit()) {
		return EXIT_FAILURE;
	}

	// disable automatic OpenGL context creation from glfw
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// create window
	_window = glfwCreateWindow(1280, 720, "opal", nullptr, nullptr);

	// load volk
	if (volkInitialize() != VK_SUCCESS) {
		return EXIT_FAILURE;
	}

	// start building vulkan instance
	vkb::InstanceBuilder instance_builder;

	instance_builder.set_app_name("app")
			.set_engine_name("Opal")
			.require_api_version(1, 0, 0);

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

	// enable surface extensions
	instance_builder.enable_extension(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef WIN32
	instance_builder.enable_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	instance_builder.enable_extension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	instance_builder.enable_extension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
	instance_builder.enable_extension(
			VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

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

	// create surface on glfw window for vulkan to draw to
	_vk_surface = nullptr;
	VkResult err = glfwCreateWindowSurface(
			_vkb_instance.instance, _window, NULL, &_vk_surface);

	if (err != VK_SUCCESS) {
		return EXIT_FAILURE;
	}

	vkb::PhysicalDeviceSelector device_selector{ _vkb_instance };

	vkb::PhysicalDevice physical_device =
			device_selector.set_minimum_version(1, 1)
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

	_device = device_builder_return.value();

	// attach volk to device
	volkLoadDevice(_device.device);

	return EXIT_SUCCESS;
}

void Renderer::start_render_loop() {

	while (!glfwWindowShouldClose(_window)) {
		glfwPollEvents();

		render();
	}
}

void Renderer::destroy() {

	vkDeviceWaitIdle(_device.device);

	vkDestroyDevice(_device.device, nullptr);

	vkDestroySurfaceKHR(_vkb_instance.instance, _vk_surface, nullptr);

	vkb::destroy_debug_utils_messenger(
			_vkb_instance.instance, _vkb_instance.debug_messenger);

	vkDestroyInstance(_vkb_instance.instance, nullptr);

	glfwDestroyWindow(_window);
	glfwTerminate();
}

void Renderer::render() {}
