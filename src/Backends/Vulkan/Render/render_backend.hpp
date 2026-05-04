#pragma once

#include "Engine/Rendering/render_backend.hpp"
#include "Backends/Vulkan/Render/render_context.hpp"
#include "Backends/Vulkan/Render/renderer.hpp"

namespace lve::backend {
  class VulkanRenderBackend final : public RenderBackend {
  public:
    VulkanRenderBackend(LveWindow &window, LveDevice &device);

    void shutdown() override;

    CommandBufferHandle beginFrame() override;
    void endFrame() override;

    void beginSwapChainRenderPass(CommandBufferHandle commandBuffer) override;
    void endSwapChainRenderPass(CommandBufferHandle commandBuffer) override;

    void ensureOffscreenTargets(
      std::uint32_t sceneWidth,
      std::uint32_t sceneHeight,
      std::uint32_t gameWidth,
      std::uint32_t gameHeight) override;

    bool wasSwapChainRecreated() const override;
    RenderPassHandle getSwapChainRenderPass() const override;
    std::size_t getSwapChainImageCount() const override;
    DescriptorSetHandle getSceneViewDescriptor() const override;
    DescriptorSetHandle getGameViewDescriptor() const override;
    float getAspectRatio() const override;
    int getFrameIndex() const override;
    RenderDebugStats getDebugStats() const override;

    void setWireframe(bool enabled) override;
    void setNormalView(bool enabled) override;

    void renderSceneView(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &objects,
      CommandBufferHandle commandBuffer) override;
    void renderGameView(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &objects,
      CommandBufferHandle commandBuffer) override;

  private:
    LveRenderer renderer;
    RenderContext renderContext;
  };
} // namespace lve::backend
