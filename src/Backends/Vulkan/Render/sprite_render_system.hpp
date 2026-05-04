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
  class SpriteRenderSystem {
  public:
    SpriteRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SpriteRenderSystem();

    SpriteRenderSystem(const SpriteRenderSystem &) = delete;
    SpriteRenderSystem &operator=(const SpriteRenderSystem &) = delete;

    void renderSprites(FrameInfo &frameInfo);
    void setBillboardMode(BillboardMode mode) { billboardMode = mode; }

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipelines(VkRenderPass renderPass);

    LveDevice &lveDevice;
    VkRenderPass renderPass;
    std::unique_ptr<LvePipeline> spritePipeline;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    std::unique_ptr<LveDescriptorSetLayout> renderSystemLayout;
    BillboardMode billboardMode{BillboardMode::None};
  };
} // namespace lve


