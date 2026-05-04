#include "Backends/Vulkan/Render/grid_render_system.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

  GridRenderSystem::GridRenderSystem(
    LveDevice &device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  GridRenderSystem::~GridRenderSystem() {
    pipeline.reset();
    if (pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
      pipelineLayout = VK_NULL_HANDLE;
    }
  }

  void GridRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GridPushConstants);

    std::array<VkDescriptorSetLayout, 1> descriptorSetLayouts{globalSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create grid pipeline layout!");
    }
  }

  void GridRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    config.bindingDescriptions.clear();
    config.attributeDescriptions.clear();
    config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    config.depthStencilInfo.depthWriteEnable = VK_FALSE;
    config.pipelineLayout = pipelineLayout;
    config.renderPass = renderPass;
    LvePipeline::enableAlphaBlending(config);

    pipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "Shaders/grid_shader.vert.spv",
      "Shaders/grid_shader.frag.spv",
      config);
  }

  void GridRenderSystem::render(FrameInfo &frameInfo) {
    if (!enabled || !pipeline) {
      return;
    }

    pipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);

    GridPushConstants push{};
    vkCmdPushConstants(
      frameInfo.commandBuffer,
      pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT,
      0,
      sizeof(GridPushConstants),
      &push);

    const int radius = static_cast<int>(push.config.x);
    const uint32_t lineCount = static_cast<uint32_t>((radius * 2 + 1) * 2);
    vkCmdDraw(frameInfo.commandBuffer, lineCount * 2u, 1, 0, 0);
  }

} // namespace lve
