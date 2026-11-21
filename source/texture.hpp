#pragma once
#include "graphics.hpp"
#include <functional>

namespace graphics {
void transfer(const std::function<void(VkCommandBuffer)>& record);
Buffer createDeviceBuffer(const void* data, size_t size, VkBufferUsageFlags usage);
struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};
Texture loadTexture(const std::filesystem::path& path, bool srgb);
void destroyTexture(Texture& texture);
}
