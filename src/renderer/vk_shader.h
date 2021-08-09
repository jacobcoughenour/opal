#ifndef __SHADER_H__
#define __SHADER_H__

#include "vk_types.h"

#include "../utils/error.h"
#include "../utils/file.h"
#include "../utils/log.h"

#include <array>

namespace Opal {

struct Shader {
	/**
	 * Optional name for debugging.
	 */
	const char *name;
	/**
	 * Shader code used to create the module.
	 */
	std::vector<char> code;
	/**
	 * The actual VkShaderModule handle.
	 */
	VkShaderModule module;

	VkShaderStageFlagBits stage;

	struct DescriptorSet {
		uint32_t set_number;
		VkDescriptorSetLayoutCreateInfo create_info;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};

	struct PushConstant {
		uint32_t size = 0;
	};

	std::vector<DescriptorSet> sets;
	PushConstant push_constant;
	VkPipelineShaderStageCreateInfo pipeline_stage;
};

static Error
_reflectShaderCode(Shader *shader, SpvReflectShaderModule spv_module) {

/**
 * Wraps spvReflectEnumerate##() in a cleaner way so you don't have to manually
 * call it twice to get the element count.
 * @param spv_module the spv reflect handle.
 * @param property spvReflectEnumerate##property() to use.
 * @param prop_type the type of each element without the SpvReflect prefix.
 * @param block your block of code that does the enumeration over the populated
 * `elements` vector. You can use `property##_count` to get the number of
 * elements and your code only runs if it is > 0.
 */
#define SPV_ENUMERATE(spv_module, property, prop_type, block)                  \
	uint32_t property##_count = 0;                                             \
	ERR_FAIL_COND_V_MSG(                                                       \
			spvReflectEnumerate##property(                                     \
					&spv_module, &property##_count, nullptr) !=                \
					SPV_REFLECT_RESULT_SUCCESS,                                \
			FAIL,                                                              \
			"Failed to enumerate SPIR-V %s",                                   \
			_STR(property));                                                   \
	if (property##_count > 0) {                                                \
		std::vector<SpvReflect##prop_type *> elements(property##_count);       \
		ERR_FAIL_COND_V_MSG(                                                   \
				spvReflectEnumerate##property(                                 \
						&spv_module, &property##_count, elements.data()) !=    \
						SPV_REFLECT_RESULT_SUCCESS,                            \
				FAIL,                                                          \
				"Failed to enumerate SPIR-V %s",                               \
				_STR(property));                                               \
		block                                                                  \
	}

	LOG_DEBUG("shader: %s", shader->name);

	// get the shader stage
	shader->stage = static_cast<VkShaderStageFlagBits>(spv_module.shader_stage);
	LOG_DEBUG("stage: %d", (int)shader->stage);

	// get all the descriptor sets
	SPV_ENUMERATE(spv_module, DescriptorSets, DescriptorSet, {
		std::vector<Shader::DescriptorSet> sets(DescriptorSets_count);

		for (uint32_t i = 0; i < DescriptorSets_count; i++) {
			const auto &refl_set = elements[i];
			auto &set			 = sets[i];

			set.set_number = refl_set->set;
			set.create_info.sType =
					VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			set.create_info.bindingCount = refl_set->binding_count;
			set.bindings.resize(refl_set->binding_count);

			for (uint32_t j = 0; j < refl_set->binding_count; j++) {
				const auto refl_binding = refl_set->bindings[j];
				auto binding			= set.bindings[j];

				binding.binding		   = refl_binding->binding;
				binding.descriptorType = static_cast<VkDescriptorType>(
						refl_binding->descriptor_type);
				binding.descriptorCount = 1;
				binding.stageFlags		= shader->stage;

				LOG_DEBUG(
						"layout(binding = %d) uniform %s",
						(int)refl_binding->binding,
						refl_binding->name);

				for (uint32_t k = 0; k < refl_binding->array.dims_count; k++) {
					binding.descriptorCount *= refl_binding->array.dims[k];
				}
			}

			set.create_info.pBindings = set.bindings.data();
		}

		// assign the sets to the shader struct
		shader->sets = sets;
	})

	// get inputs from vertex shaders
	SPV_ENUMERATE(spv_module, InputVariables, InterfaceVariable, {
		for (uint32_t i = 0; i < InputVariables_count; i++) {
			const auto &input_var = elements[i];

			LOG_DEBUG(
					"layout(location = %d) in %s",
					input_var->location,
					input_var->name);
		}
	})

	// get outputs from fragment shaders
	SPV_ENUMERATE(spv_module, OutputVariables, InterfaceVariable, {
		for (uint32_t i = 0; i < OutputVariables_count; i++) {
			const auto &output_var = elements[i];

			LOG_DEBUG(
					"layout(location = %d) out %s",
					output_var->location,
					output_var->name);
		}
	})

	// todo do we need inputs and outputs for other stages?

	// get the push constants
	SPV_ENUMERATE(spv_module, PushConstantBlocks, BlockVariable, {
		// todo support multiple push constants

		ERR_FAIL_COND_V_MSG(
				PushConstantBlocks_count > 1,
				FAIL,
				"Only single push constants are supported right now");

		const auto push_constant = elements[0];

		LOG_DEBUG("layout(push_constant) uniform %s {", push_constant->name);

		shader->push_constant.size = push_constant->size;
		// shader->push_constant.stage = shad
	})

	return OK;
}

/**
 *  @brief Create shader module from spirv code.
 */
static Error createShader(
		VkDevice device,
		Shader *shader,
		const std::string &name,
		const std::vector<char> &code) {

	VkShaderModuleCreateInfo shader_info {
		.sType	  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode	  = reinterpret_cast<const uint32_t *>(code.data()),
	};

	shader->name = name.c_str();
	shader->code = code;

	// reflect the shader code to gather more info

	SpvReflectShaderModule spv_module;
	SpvReflectResult result = spvReflectCreateShaderModule(
			shader_info.codeSize, shader_info.pCode, &spv_module);

	ERR_FAIL_COND_V_MSG(
			result != SPV_REFLECT_RESULT_SUCCESS,
			FAIL,
			"Failed to create reflection module for shader");

	auto reflect_result = _reflectShaderCode(shader, spv_module);

	// always destroy the reflect module
	spvReflectDestroyShaderModule(&spv_module);

	ERR_FAIL_COND_V_MSG(
			reflect_result != OK,
			FAIL,
			"Failed to reflect SPIR-V code for shader");

	// todo build out the graphics pipeline

	// create the shader module with vulkan.
	if (vkCreateShaderModule(device, &shader_info, nullptr, &shader->module) !=
		VK_SUCCESS) {
		LOG_ERR("Failed to create shader module: %s", name.c_str());
		return FAIL;
	}

	// setup for graphics pipeline creation later

	VkPipelineShaderStageCreateInfo shader_stage_info {
		.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage	= shader->stage,
		.module = shader->module,
		// entry point name.
		.pName = "main",
	};
	shader->pipeline_stage = shader_stage_info;

	return OK;
}

/**
 *  @brief Create shader from spirv file.
 */
static Error createShaderFromFile(
		VkDevice device, Shader *shader, const std::string &filename) {
	auto code = readFile(filename + ".spv");
	return createShader(device, shader, filename, code);
}

/**
 * @brief Destroy shader
 */
static void destroyShader(VkDevice device, Shader *shader) {
	vkDestroyShaderModule(device, shader->module, nullptr);
}

} // namespace Opal

#endif // __SHADER_H__