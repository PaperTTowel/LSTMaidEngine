#pragma once

#include "Backends/Vulkan/Core/descriptors.hpp"
#include "Backends/Vulkan/Core/device.hpp"
#include "Backends/Vulkan/Core/buffer.hpp"
#include "Backends/Vulkan/Render/frame_info.hpp"
#include "Backends/Vulkan/Render/grid_render_system.hpp"
#include "Backends/Vulkan/Render/point_light_system.hpp"
#include "Backends/Vulkan/Render/renderer.hpp"
#include "Backends/Vulkan/Render/simple_render_system.hpp"
#include "Backends/Vulkan/Render/sprite_render_system.hpp"

// std
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace lve {
  class RenderContext {
  public:
    RenderContext(LveDevice &device, LveRenderer &renderer);
    ~RenderContext();

    void shutdown();

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
    bool beginSceneViewRenderPass(VkCommandBuffer commandBuffer);
    void endSceneViewRenderPass(VkCommandBuffer commandBuffer);
    bool beginGameViewRenderPass(VkCommandBuffer commandBuffer);
    void endGameViewRenderPass(VkCommandBuffer commandBuffer);
    void ensureOffscreenTargets(uint32_t sceneWidth, uint32_t sceneHeight, uint32_t gameWidth, uint32_t gameHeight);

    bool wasSwapChainRecreated() const;
    void clearSwapChainRecreated();
    VkRenderPass getSwapChainRenderPass() const;
    size_t getSwapChainImageCount() const { return lveRenderer.getSwapChainImageCount(); }
    VkDescriptorSet getSceneViewDescriptor() const;
    VkDescriptorSet getGameViewDescriptor() const;
    VkExtent2D getSceneViewExtent() const;
    VkExtent2D getGameViewExtent() const;
    backend::RenderDebugStats getDebugStats() const;

    FrameInfo makeFrameInfo(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &gameObjects,
      VkCommandBuffer commandBuffer,
      int viewIndex);
    void updateGlobalUbo(int frameIndex, int viewIndex, const GlobalUbo &ubo);

    SimpleRenderSystem &simpleSystem() { return *simpleRenderSystem; }
    SpriteRenderSystem &spriteSystem() { return *spriteRenderSystem; }
    PointLightSystem &pointLightSystem() { return *pointLightSystemPtr; }
    GridRenderSystem &gridSystem() { return *gridRenderSystem; }

  private:
    struct OffscreenTarget {
      VkExtent2D extent{};
      VkImage colorImage{VK_NULL_HANDLE};
      VkDeviceMemory colorMemory{VK_NULL_HANDLE};
      VkImageView colorView{VK_NULL_HANDLE};
      VkImage depthImage{VK_NULL_HANDLE};
      VkDeviceMemory depthMemory{VK_NULL_HANDLE};
      VkImageView depthView{VK_NULL_HANDLE};
      VkFramebuffer framebuffer{VK_NULL_HANDLE};
      VkSampler sampler{VK_NULL_HANDLE};
      VkDescriptorSet imguiDescriptor{VK_NULL_HANDLE};
    };

    struct RetiredOffscreenTarget {
      OffscreenTarget target{};
      int framesRemaining{2};
    };

    void createBuffersAndDescriptors();
    void createOffscreenRenderPass();
    void destroyOffscreenRenderPass();
    void destroyOffscreenTarget(OffscreenTarget &target);
    void retireOffscreenTarget(OffscreenTarget &target);
    void destroyRetiredOffscreenTargets();
    void collectRetiredOffscreenTargets();
    void createOffscreenTarget(OffscreenTarget &target, VkExtent2D extent, const char *debugName);
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer, const OffscreenTarget &target);
    void createRenderSystems();
    void resetObjectDescriptorPools();

    LveDevice &lveDevice;
    LveRenderer &lveRenderer;

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::array<std::unique_ptr<LveDescriptorPool>, backend::kMaxFramesInFlight> objectDescriptorPools{};
    FrameDescriptorCache descriptorCache{};
    std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
    std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
    std::vector<VkDescriptorSet> globalDescriptorSets;

    std::unique_ptr<SimpleRenderSystem> simpleRenderSystem;
    std::unique_ptr<SpriteRenderSystem> spriteRenderSystem;
    std::unique_ptr<PointLightSystem> pointLightSystemPtr;
    std::unique_ptr<GridRenderSystem> gridRenderSystem;

    VkRenderPass offscreenRenderPass{VK_NULL_HANDLE};
    VkFormat offscreenColorFormat{VK_FORMAT_UNDEFINED};
    VkFormat offscreenDepthFormat{VK_FORMAT_UNDEFINED};
    OffscreenTarget sceneViewTarget{};
    OffscreenTarget gameViewTarget{};
    std::vector<RetiredOffscreenTarget> retiredOffscreenTargets{};
    bool swapChainRecreated{false};
    bool shutdownComplete{false};
  };
} // namespace lve


