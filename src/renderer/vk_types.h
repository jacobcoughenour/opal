#ifndef __VK_TYPES_H__
#define __VK_TYPES_H__

/**
 * This header takes care of all the configuration between thirdparty libraries
 * and Vulkan so you don't have to. You should always use this header when
 * doing something Vulkan related in the renderer.
 */

// renderer config defines
#include "config.h"

// #define VMA_IMPLEMENTATION
// #define VMA_STATIC_VULKAN_FUNCTIONS 0
// #include "vk_mem_alloc.h"
#include "../../thirdparty/vulkan-memory-allocator/vk_mem_alloc.h"

/**
 * volk is used to load in Vulkan.
 * https://github.com/zeux/volk
 */
#include <volk.h>

/**
 * VkBootstrap wraps some of the tedious Vulkan initialization stuff.
 * https://github.com/charles-lunarg/vk-bootstrap
 */
#include <VkBootstrap.h>

/**
 * SPIRV-Reflect provides reflection for spirv shader code.
 * https://github.com/KhronosGroup/SPIRV-Reflect
 */
// #include "spirv-reflect/spirv_reflect.h"
#include "../../thirdparty/spirv-reflect/spirv_reflect.h"

/**
 * GLFW handles the window and inputs.
 * https://www.glfw.org
 */
#include <GLFW/glfw3.h>

/**
 * GLM adds a lot of math stuff.
 * https://github.com/g-truc/glm
 */
// GLM uses [-1, 1] for depth but we need [0, 1] for Vulkan.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// We're just importing the core types here.
#include <glm/glm.hpp>

#endif // __VK_TYPES_H__