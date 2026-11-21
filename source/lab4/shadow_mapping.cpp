#include "shadow_mapping.hpp"
#include "floor.hpp"
#include "../texture.hpp"
#include "../textured_renderer.hpp"
#include <cstring>
#include <stdexcept>

namespace shadows {
namespace {
Settings shadowSettings;
struct Uniform {
    glm::mat4 sun{1.f}, spot{1.f};
    glm::ivec4 options{0};
};
static_assert(sizeof(Uniform) == 144);
Uniform uniform;
graphics::Buffer uniformBuffer;
graphics::Texture maps[2];
bool initialized[2]{};
VkFormat format = VK_FORMAT_UNDEFINED;
VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
VkDescriptorPool pool = VK_NULL_HANDLE;
VkDescriptorSet set = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkPipeline pipeline = VK_NULL_HANDLE;

glm::mat4 lightView(glm::vec3 position, glm::vec3 direction) {
    direction = lighting::safeDirection(direction);
    const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    return glm::lookAtRH(position, position + direction, up);
}
void createMap(graphics::Texture& map) {
    const auto& c = graphics::internal::context;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

    //Создаётся двумерная текстура квадратного размера
    image.imageType = VK_IMAGE_TYPE_2D; 
    image.format = format;
    image.extent = {resolution,resolution,1}; 
    
    image.mipLevels = image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT; image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VmaAllocationCreateInfo allocation{}; allocation.usage = VMA_MEMORY_USAGE_AUTO;
    graphics::check(vmaCreateImage(c.allocator,&image,&allocation,&map.image,&map.allocation,nullptr),"shadow image");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = map.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = format;
    view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
    graphics::check(vkCreateImageView(c.device,&view,nullptr,&map.view),"shadow view");
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST; // читать ближайший texel без интерполяции соседних значений.
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; // при выходе за границы текстуры использовать граничное значение.
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // Белый цвет границы соответствует значению 1.0 для глубины.
    sampler.compareEnable = VK_TRUE; sampler.compareOp = VK_COMPARE_OP_LESS; // включает сравнение глубины
    graphics::check(vkCreateSampler(c.device,&sampler,nullptr,&map.sampler),"shadow comparison sampler"); // задаёт правило сравнения: переданная в sampler глубина должна быть меньше значения из карты.
}
void createPipeline(const std::filesystem::path& directory) {
    const auto device = graphics::internal::context.device;
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(glm::mat4)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1; layoutInfo.pPushConstantRanges = &range;
    graphics::check(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&pipelineLayout),"shadow pipeline layout");
    VkShaderModule vertex = graphics::loadShader(directory/"shadow.vert.spv");
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT; stage.module = vertex; stage.pName = "main";
    VkVertexInputBindingDescription binding{0,sizeof(texturing::Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    input.vertexBindingDescriptionCount = input.vertexAttributeDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding; input.pVertexAttributeDescriptions = &attribute;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1.f;
    raster.depthBiasEnable = VK_TRUE; // разрешает использование Depth Bias
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE; depth.depthCompareOp = VK_COMPARE_OP_LESS; // Запись глубины
    const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 3; dynamic.pDynamicStates = states;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.depthAttachmentFormat = format;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pNext = &rendering; info.stageCount = 1; info.pStages = &stage;
    info.pVertexInputState = &input; info.pInputAssemblyState = &assembly; info.pViewportState = &viewport;
    info.pRasterizationState = &raster; info.pMultisampleState = &samples; info.pDepthStencilState = &depth;
    info.pDynamicState = &dynamic; info.layout = pipelineLayout;
    const VkResult result = vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline);
    vkDestroyShaderModule(device,vertex,nullptr);
    graphics::check(result,"shadow dynamic pipeline");
}
}
void initialize(const std::filesystem::path& directory) {
    const auto& c = graphics::internal::context;
    for (VkFormat candidate : {VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(c.physical_device,candidate,&properties);
        const auto required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) { format = candidate; break; }
    }
    if (format == VK_FORMAT_UNDEFINED) throw std::runtime_error("No sampleable shadow depth format");
    for (auto& map : maps) createMap(map);
    uniformBuffer = graphics::createBuffer(&uniform,sizeof(uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const VkDescriptorSetLayoutBinding bindings[] = {
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 3; layoutInfo.pBindings = bindings;
    graphics::check(vkCreateDescriptorSetLayout(c.device,&layoutInfo,nullptr,&setLayout),"shadow descriptor layout");
    const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2}};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1; poolInfo.poolSizeCount = 2; poolInfo.pPoolSizes = sizes;
    graphics::check(vkCreateDescriptorPool(c.device,&poolInfo,nullptr,&pool),"shadow descriptor pool");
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = pool; alloc.descriptorSetCount = 1; alloc.pSetLayouts = &setLayout;
    graphics::check(vkAllocateDescriptorSets(c.device,&alloc,&set),"shadow descriptor set");
    VkDescriptorBufferInfo buffer{uniformBuffer.handle,0,sizeof(uniform)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer;
    vkUpdateDescriptorSets(c.device,1,&write,0,nullptr);
    for (uint32_t i = 0; i < 2; ++i) {
        VkDescriptorImageInfo image{maps[i].sampler,maps[i].view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        write.dstBinding = i+1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pBufferInfo = nullptr; write.pImageInfo = &image;
        vkUpdateDescriptorSets(c.device,1,&write,0,nullptr);
    }
    createPipeline(directory);
}
VkDescriptorSetLayout layout() { return setLayout; }
Settings& settings() { return shadowSettings; }
void bind(VkCommandBuffer cmd, VkPipelineLayout lightingLayout) {
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,lightingLayout,2,1,&set,0,nullptr);
}
void update(const texturing::State& s) {
    const auto& settings = shadowSettings;
    const glm::vec3 direction = lighting::safeDirection(s.direction);
    const float half = std::max(settings.coverage,1.f)*0.5f;
    auto projection = glm::orthoRH_ZO(-half,half,-half,half,0.1f,60.f);
    projection[1][1] *= -1.f;
    uniform.sun = projection * lightView(-direction*25.f,direction);
    uniform.spot = glm::mat4(1.f);
    uniform.options = {settings.enabled && s.directionalEnabled ? 1 : 0,-1,settings.pcf ? 1 : 0,0}; // Третий компонент options.z передаёт в шейдер настройку включения PCF.
    if (settings.spotlight && settings.spotIndex >= 0 && settings.spotIndex < int(s.spots.size())) {
        const auto& spot = s.spots[settings.spotIndex];
        if (spot.enabled) {
            auto p = glm::perspectiveRH_ZO(glm::radians(std::clamp(spot.outerAngle*2.f,1.f,160.f)),1.f,0.1f,80.f);
            p[1][1] *= -1.f;
            uniform.spot = p * lightView(spot.position,spot.direction);
            int compactIndex = 0;
            for (int i = 0; i < settings.spotIndex; ++i) if (s.spots[i].enabled) ++compactIndex;
            if (settings.enabled) uniform.options.y = compactIndex;
        }
    }
    void* mapped = nullptr;
    const auto allocator = graphics::internal::context.allocator;
    graphics::check(vmaMapMemory(allocator,uniformBuffer.allocation,&mapped),"map shadow uniform");
    std::memcpy(mapped,&uniform,sizeof(uniform));
    const VkResult result = vmaFlushAllocation(allocator,uniformBuffer.allocation,0,sizeof(uniform));
    vmaUnmapMemory(allocator,uniformBuffer.allocation);
    graphics::check(result,"flush shadow uniform");
}
void record(VkCommandBuffer cmd, const graphics::Buffer& pyramid, uint32_t count,
            const glm::mat4& model) {
    const auto& settings = shadowSettings;
    for (int i = 0; i < 2; ++i) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = initialized[i] ? VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = initialized[i] ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = maps[i].image; barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        vkCmdPipelineBarrier(cmd,initialized[i] ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = maps[i].view; attachment.imageLayout = barrier.newLayout;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.depthStencil = {1.f,0};
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea.extent = {resolution,resolution}; rendering.layerCount = 1;
        rendering.pDepthAttachment = &attachment;
        vkCmdBeginRendering(cmd,&rendering);
        VkViewport viewport{0,0,float(resolution),float(resolution),0,1};
        VkRect2D scissor{{0,0},{resolution,resolution}};
        vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
        vkCmdSetDepthBias(cmd,settings.depthBias,0.f,settings.slopeBias);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        const auto vp = i == 0 ? uniform.sun : uniform.spot;
        const auto mvp = vp * model;
        VkDeviceSize offset = 0;
        vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(mvp),&mvp);
        vkCmdBindVertexBuffers(cmd,0,1,&pyramid.handle,&offset);
        vkCmdDraw(cmd,count,1,0,0);
        lab4::floor::recordShadow(cmd,pipelineLayout,vp);
        vkCmdEndRendering(cmd);
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        initialized[i] = true;
    }
}
void shutdown() {
    const auto device = graphics::internal::context.device;
    vkDeviceWaitIdle(device);
    vkDestroyPipeline(device,pipeline,nullptr);
    vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
    vkDestroyDescriptorPool(device,pool,nullptr);
    vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
    graphics::destroyBuffer(uniformBuffer);
    for (auto& map : maps) graphics::destroyTexture(map);
}
}
