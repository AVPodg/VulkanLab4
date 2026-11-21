#pragma once
#include <VkBootstrap.h>

namespace lab4 {
void configureInstance(vkb::InstanceBuilder& builder);
void configureDevice(vkb::PhysicalDeviceSelector& selector);
}
