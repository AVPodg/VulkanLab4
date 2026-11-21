#pragma once
#include "../texture.hpp"
#include "../textured_renderer.hpp"
#include <array>

namespace lab4::floor {
struct Settings {
    bool enabled = true;
    int material = 0;
};
Settings& settings();
using TextureMaps = std::array<graphics::Texture, texturing::materialCount>;
void initialize(VkDescriptorSetLayout modelLayout, const TextureMaps& albedo,
                const TextureMaps& specular, const TextureMaps& emissive);
void shutdown();
void prepare();
void recordShadow(VkCommandBuffer cmd, VkPipelineLayout layout, const glm::mat4& lightVP);
void render(VkCommandBuffer cmd, VkPipelineLayout layout, VkPipeline pipeline);
}
