#ifndef __VK_DEBUG_H__
#define __VK_DEBUG_H__

#include "vk_types.h"

namespace Opal {
class VkDebug {

public:
	static inline void object_name(
			VkDevice device,
			VkObjectType type,
			uint64_t handle,
			const char *name) {
#ifdef USE_DEBUG_UTILS
		VkDebugUtilsObjectNameInfoEXT name_info {
			.sType		  = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.pNext		  = NULL,
			.objectType	  = type,
			.objectHandle = handle,
			.pObjectName  = name,
		};
		vkSetDebugUtilsObjectNameEXT(device, &name_info);
#else
		(void)0;
#endif
	}

	static inline void
	begin_label(VkCommandBuffer command_buffer, const char *name) {
#ifdef USE_DEBUG_UTILS
		VkDebugUtilsLabelEXT label_info {
			.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pNext		= NULL,
			.pLabelName = name,
		};
		vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
#else
		(void)0;
#endif
	}

	static inline void
	insert_label(VkCommandBuffer command_buffer, const char *name) {
#ifdef USE_DEBUG_UTILS
		VkDebugUtilsLabelEXT label_info {
			.sType		= VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pNext		= NULL,
			.pLabelName = name,
		};
		vkCmdInsertDebugUtilsLabelEXT(command_buffer, &label_info);
#else
		(void)0;
#endif
	}

	static inline void end_label(VkCommandBuffer command_buffer) {
#ifdef USE_DEBUG_UTILS
		vkCmdEndDebugUtilsLabelEXT(command_buffer);
#else
		(void)0;
#endif
	}
};

} // namespace Opal

#endif // __VK_DEBUG_H__