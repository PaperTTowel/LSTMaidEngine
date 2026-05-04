#pragma once

#include "Backends/Vulkan/Core/device.hpp"
#include "Backends/Vulkan/Core/pipeline.hpp"
#include "Backends/Vulkan/Render/frame_info.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace lve {

  class GridRenderSystem {
  public:
    GridRenderSystem(
      LveDevice &device,
      VkRenderPass renderPass,
      VkDescriptorSetLayout globalSetLayout);
    ~GridRenderSystem();

    void setEnabled(bool value) { enabled = value; }
    bool isEnabled() const { return enabled; }
    void render(FrameInfo &frameInfo);

  private:
    struct GridPushConstants {
      glm::vec4 config{20.f, 1.f, 5.f, 0.f};
      glm::vec4 minorColor{0.28f, 0.31f, 0.36f, 0.35f};
      glm::vec4 majorColor{0.42f, 0.46f, 0.52f, 0.55f};
      glm::vec4 axisXColor{0.72f, 0.22f, 0.22f, 0.75f};
      glm::vec4 axisZColor{0.22f, 0.38f, 0.72f, 0.75f};
    };

    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    LveDevice &lveDevice;
    std::unique_ptr<LvePipeline> pipeline{};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    bool enabled{true};
  };

} // namespace lve
