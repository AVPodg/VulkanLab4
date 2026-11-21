#include "vulkan_requirements.hpp"

namespace lab4 {
void configureInstance(vkb::InstanceBuilder& builder) {
    builder.require_api_version(VK_MAKE_VERSION(1,3,0));
}
void configureDevice(vkb::PhysicalDeviceSelector& selector) {
    VkPhysicalDeviceVulkan13Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = VK_TRUE;
    selector.set_minimum_version(1,3).set_required_features_13(features);
}
}
