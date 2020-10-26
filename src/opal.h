#ifndef OPAL_OPAL_H
#define OPAL_OPAL_H

#include <nvvk/appbase_vkpp.hpp>


class Opal : public nvvk::AppBase {

public:

	void setup(const vk::Instance &instance, const vk::Device &device, const vk::PhysicalDevice &physicalDevice, uint32_t graphicsQueueIndex) override;


};

#endif //OPAL_OPAL_H
