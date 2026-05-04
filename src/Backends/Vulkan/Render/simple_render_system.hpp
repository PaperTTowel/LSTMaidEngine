#pragma once

#include "Engine/Core/camera.hpp"
#include "Engine/Scene/game_object.hpp"
#include "Backends/Vulkan/Core/pipeline.hpp"
#include "Backends/Vulkan/Render/frame_info.hpp"
#include "Backends/Vulkan/Core/device.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
  class SimpleRenderSystem {
  public:
    SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    void setWireframe(bool enabled);
    bool isWireframeEnabled() const { return wireframeEnabled; }
    void setNormalView(bool enabled) { normalViewEnabled = enabled; }
    bool isNormalView() const { return normalViewEnabled; }

    SimpleRenderSystem(const LveWindow &) = delete;
    SimpleRenderSystem &operator=(const LveWindow &) = delete;

    void renderGameObjects(FrameInfo &frameInfo);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipelines(VkRenderPass renderPass);

    LveDevice &lveDevice;
    VkRenderPass renderPass;
    std::unique_ptr<LvePipeline> fillPipeline;
    std::unique_ptr<LvePipeline> wireframePipeline;
    VkPipelineLayout pipelineLayout;

    std::unique_ptr<LveDescriptorSetLayout> renderSystemLayout;
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
  };
} // namespace lve


