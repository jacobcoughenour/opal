#ifndef __SHADER_H__
#define __SHADER_H__

#include <volk.h>

#include "../utils/file.h"
#include "../utils/log.h"

namespace Opal {

/**
 *  @brief Create shader module from spirv code.
 */
static VkShaderModule createShaderModule(VkDevice device,
		const std::string &name,
		const std::vector<char> &code) {

	VkShaderModuleCreateInfo shader_info = {};
	// clang-format off
	shader_info.sType		= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.codeSize	= code.size();
	shader_info.pCode		= reinterpret_cast<const uint32_t *>(code.data());
	// clang-format on

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &shader_info, nullptr, &shader_module) !=
			VK_SUCCESS) {
		LOG_ERR("Failed to create shader module: %s", name);
		return VK_NULL_HANDLE;
	}
	return shader_module;
}

/**
 *  @brief Create shader module from spirv file.
 */
static VkShaderModule createShaderModuleFromFile(
		VkDevice device, const std::string &filename) {
	auto code = readFile(filename + ".spv");
	return createShaderModule(device, filename, code);
}

} // namespace Opal

#endif // __SHADER_H__