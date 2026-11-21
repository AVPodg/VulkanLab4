#include "floor.hpp"
#include <cstring>

namespace lab4::floor {
namespace {
Settings options;
graphics::Buffer vertices, uniformBuffer;
VkDescriptorPool pool = VK_NULL_HANDLE;
std::array<VkDescriptorSet, texturing::materialCount> materialSets{};
// Same model UBO contract as the existing textured renderer and shaders.
struct ModelUniform {
    glm::mat4 transform{1.f}, normalMatrix{1.f};
    glm::vec4 albedo{0.65f,0.65f,0.65f,1.f};
    glm::vec4 specular{0.08f,0.08f,0.08f,1.f};
    glm::vec4 parameters{32.f,0.f,0.f,0.f};
    glm::vec4 maps{0.f,1.f,0.f,0.f};
};
static_assert(sizeof(ModelUniform) == 192 && offsetof(ModelUniform, maps) == 176);
void draw(VkCommandBuffer cmd) {
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd,0,1,&vertices.handle,&offset);
    vkCmdDraw(cmd,6,1,0,0);
}
}
Settings& settings() { return options; }
void initialize(VkDescriptorSetLayout modelLayout, const TextureMaps& albedo,
                const TextureMaps& specular, const TextureMaps& emissive) {
    const texturing::Vertex mesh[] = {
        {{-6,-1.05f,-6},{1,1,1},{0,1,0},{0,0}},
        {{-6,-1.05f, 6},{1,1,1},{0,1,0},{0,1}},
        {{ 6,-1.05f, 6},{1,1,1},{0,1,0},{1,1}},
        {{-6,-1.05f,-6},{1,1,1},{0,1,0},{0,0}},
        {{ 6,-1.05f, 6},{1,1,1},{0,1,0},{1,1}},
        {{ 6,-1.05f,-6},{1,1,1},{0,1,0},{1,0}}};
    vertices = graphics::createDeviceBuffer(mesh,sizeof(mesh),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const ModelUniform uniform;
    uniformBuffer = graphics::createBuffer(&uniform,sizeof(uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const auto device = graphics::internal::context.device;
    const VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,texturing::materialCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,texturing::materialCount*3}};
    VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.maxSets = texturing::materialCount; info.poolSizeCount = 2; info.pPoolSizes = sizes;
    graphics::check(vkCreateDescriptorPool(device,&info,nullptr,&pool),"floor descriptor pool");
    std::array<VkDescriptorSetLayout,texturing::materialCount> layouts;
    layouts.fill(modelLayout);
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = pool; allocation.descriptorSetCount = texturing::materialCount;
    allocation.pSetLayouts = layouts.data();
    graphics::check(vkAllocateDescriptorSets(device,&allocation,materialSets.data()),"floor material sets");
    const VkDescriptorBufferInfo buffer{uniformBuffer.handle,0,sizeof(ModelUniform)};
    for (int i = 0; i < texturing::materialCount; ++i) {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = materialSets[i]; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; write.pBufferInfo = &buffer;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        const graphics::Texture* textures[]{&albedo[i],&specular[i],&emissive[i]};
        for (uint32_t j = 0; j < 3; ++j) {
            const VkDescriptorImageInfo image{textures[j]->sampler,textures[j]->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            write.dstBinding = j+1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pBufferInfo = nullptr; write.pImageInfo = &image;
            vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        }
    }
}
void prepare() {
    if (!options.enabled) return;
    ModelUniform uniform;
    uniform.maps.x = std::clamp(options.material,0,texturing::materialCount-1) != 0 ? 1.f : 0.f;
    const auto allocator = graphics::internal::context.allocator;
    void* mapped = nullptr;
    graphics::check(vmaMapMemory(allocator,uniformBuffer.allocation,&mapped),"map floor uniform");
    std::memcpy(mapped,&uniform,sizeof(uniform));
    const auto result = vmaFlushAllocation(allocator,uniformBuffer.allocation,0,sizeof(uniform));
    vmaUnmapMemory(allocator,uniformBuffer.allocation);
    graphics::check(result,"flush floor uniform");
}
void recordShadow(VkCommandBuffer cmd, VkPipelineLayout layout, const glm::mat4& lightVP) {
    if (!options.enabled) return;
    vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(lightVP),&lightVP);
    draw(cmd);
}
void render(VkCommandBuffer cmd, VkPipelineLayout layout, VkPipeline pipeline) {
    if (!options.enabled) return;
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    const auto set = materialSets[std::clamp(options.material,0,texturing::materialCount-1)];
    const uint32_t offset = 0;
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,1,&set,1,&offset);
    draw(cmd);
}
void shutdown() {
    vkDestroyDescriptorPool(graphics::internal::context.device,pool,nullptr);
    pool = VK_NULL_HANDLE;
    graphics::destroyBuffer(uniformBuffer);
    graphics::destroyBuffer(vertices);
}
}
