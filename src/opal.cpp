#include "opal.h"


void Opal::setup(
		const vk::Instance &instance,
		const vk::Device &device,
		const vk::PhysicalDevice &physicalDevice,
		uint32_t graphicsQueueIndex) {
	AppBase::setup(instance, device, physicalDevice, graphicsQueueIndex);
}

