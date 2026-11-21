#pragma once
#include "../graphics.hpp"
#include <glm/glm.hpp>

namespace texturing { struct State; }

namespace shadows {
struct Settings {
    bool enabled = true, spotlight = true, pcf = true;
    int spotIndex = 0;
    float coverage = 14.f, depthBias = 1.25f, slopeBias = 1.75f;
};
Settings& settings();
void initialize(const std::filesystem::path& directory);
void shutdown();
VkDescriptorSetLayout layout();
void bind(VkCommandBuffer cmd, VkPipelineLayout lightingLayout);
// Called after the preceding frame's fence, before recording the next frame.
void update(const texturing::State& state);
void record(VkCommandBuffer cmd, const graphics::Buffer& pyramid, uint32_t count,
            const glm::mat4& model);
inline constexpr uint32_t resolution = 2048;
}
