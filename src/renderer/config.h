#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <volk.h>
#include <vector>

// WINDOW SETTINGS

#define WINDOW_TITLE "Opal"
#define WINDOW_INIT_SIZE 1280, 720

// VULKAN SETTINGS

#define VK_APP_NAME "Opal Demo"
#define VK_ENGINE_NAME "Opal"

#define MAX_FRAMES_IN_FLIGHT 2

// enables vulkan validation layers
#define USE_VALIDATION_LAYERS

// enables debug utils for labels
#define USE_DEBUG_UTILS

// required vulkan api version
#define VK_REQUIRED_API_VERSION VK_API_VERSION_1_1

// minimum vulkan api version supported by device
#define VK_DEVICE_MINIMUM_VERSION VK_API_VERSION_1_1

// vulkan instance extensions
const std::vector<const char *> VK_INSTANCE_EXTENSIONS = {
#ifdef USE_DEBUG_UTILS
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef WIN32
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
	VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};

// vulkan device extensions required to run
const std::vector<const char *> VK_REQUIRED_DEVICE_EXTENSIONS = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,

	// raytracing extensions
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

// vulkan device extensions optional to run
const std::vector<const char *> VK_OPTIONAL_DEVICE_EXTENSIONS = {};

// vulkan device features required to run
const VkPhysicalDeviceFeatures VK_REQUIRED_DEVICE_FEATURES{

	.samplerAnisotropy = VK_TRUE
};
const VkPhysicalDeviceVulkan11Features VK_REQUIRED_DEVICE_FEATURES_11 = {};
const VkPhysicalDeviceVulkan12Features VK_REQUIRED_DEVICE_FEATURES_12 = {};

#endif // __CONFIG_H__