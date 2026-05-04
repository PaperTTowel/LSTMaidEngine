#include "render_context.hpp"

#include "Engine/Scene/game_object.hpp"

#include <backends/imgui_impl_vulkan.h>

// std
#include <array>
#include <stdexcept>

namespace lve {

  RenderContext::RenderContext(LveDevice &device, LveRenderer &renderer)
    : lveDevice{device},
      lveRenderer{renderer} {
    createBuffersAndDescriptors();
    createOffscreenRenderPass();
    createRenderSystems();
  }

  RenderContext::~RenderContext() {
    shutdown();
  }

  void RenderContext::shutdown() {
    if (shutdownComplete) {
      return;
    }
    vkDeviceWaitIdle(lveDevice.device());
    destroyOffscreenTarget(sceneViewTarget);
    destroyOffscreenTarget(gameViewTarget);
    destroyRetiredOffscreenTargets();
    destroyOffscreenRenderPass();
    shutdownComplete = true;
  }

  void RenderContext::createBuffersAndDescriptors() {
    const uint32_t globalSetCount = backend::kMaxFramesInFlight * RENDER_VIEW_COUNT;
    globalPool = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(globalSetCount)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, globalSetCount)
      .build();

    constexpr uint32_t kMaxDescriptorSetsPerObject = 16;
    const uint32_t maxObjectSets =
      LveGameObjectManager::MAX_GAME_OBJECTS * kMaxDescriptorSetsPerObject;
    constexpr uint32_t kMaterialTextureCount = 5;
    for (auto &pool : objectDescriptorPools) {
      pool = LveDescriptorPool::Builder(lveDevice)
        .setMaxSets(maxObjectSets)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxObjectSets * kMaterialTextureCount)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxObjectSets)
        .build();
    }

    uboBuffers.resize(globalSetCount);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      uboBuffers[i]->map();
    }

    globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
      .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
      .build();

    globalDescriptorSets.resize(globalSetCount);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      if (!LveDescriptorWriter(*globalSetLayout, *globalPool)
              .writeBuffer(0, &bufferInfo)
              .build(globalDescriptorSets[i])) {
        throw std::runtime_error("failed to build global descriptor set");
      }
    }
  }

  void RenderContext::createRenderSystems() {
    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
      lveDevice,
      offscreenRenderPass,
      globalSetLayout->getDescriptorSetLayout());
    spriteRenderSystem = std::make_unique<SpriteRenderSystem>(
      lveDevice,
      offscreenRenderPass,
      globalSetLayout->getDescriptorSetLayout());
    pointLightSystemPtr = std::make_unique<PointLightSystem>(
      lveDevice,
      offscreenRenderPass,
      globalSetLayout->getDescriptorSetLayout());
    gridRenderSystem = std::make_unique<GridRenderSystem>(
      lveDevice,
      offscreenRenderPass,
      globalSetLayout->getDescriptorSetLayout());
  }

  VkCommandBuffer RenderContext::beginFrame() {
    if (shutdownComplete) {
      return VK_NULL_HANDLE;
    }
    auto commandBuffer = lveRenderer.beginFrame();
    if (lveRenderer.wasSwapChainRecreated()) {
      vkDeviceWaitIdle(lveDevice.device());
      resetObjectDescriptorPools();
      descriptorCache.clear();
      destroyRetiredOffscreenTargets();
      destroyOffscreenTarget(sceneViewTarget);
      destroyOffscreenTarget(gameViewTarget);
      destroyOffscreenRenderPass();
      createOffscreenRenderPass();
      createRenderSystems();
      swapChainRecreated = true;
    } else if (commandBuffer) {
      const int frameIndex = lveRenderer.getFrameindex();
      if (objectDescriptorPools[frameIndex]) {
        objectDescriptorPools[frameIndex]->resetPool();
      }
      descriptorCache.clearFrame(frameIndex);
    }
    return commandBuffer;
  }

  void RenderContext::endFrame() {
    lveRenderer.endFrame();
    collectRetiredOffscreenTargets();
  }

  void RenderContext::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    lveDevice.beginDebugLabel(commandBuffer, "Swapchain Render Pass", 0.25f, 0.35f, 0.95f);
    lveRenderer.beginSwapChainRenderPass(commandBuffer);
  }

  void RenderContext::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    lveRenderer.endSwapChainRenderPass(commandBuffer);
    lveDevice.endDebugLabel(commandBuffer);
  }

  void RenderContext::beginOffscreenRenderPass(VkCommandBuffer commandBuffer, const OffscreenTarget &target) {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = offscreenRenderPass;
    renderPassInfo.framebuffer = target.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = target.extent;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(target.extent.width);
    viewport.height = static_cast<float>(target.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, target.extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  bool RenderContext::beginSceneViewRenderPass(VkCommandBuffer commandBuffer) {
    if (sceneViewTarget.framebuffer == VK_NULL_HANDLE) return false;
    lveDevice.beginDebugLabel(commandBuffer, "Scene View Render Pass", 0.15f, 0.65f, 0.95f);
    beginOffscreenRenderPass(commandBuffer, sceneViewTarget);
    return true;
  }

  void RenderContext::endSceneViewRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
    lveDevice.endDebugLabel(commandBuffer);
  }

  bool RenderContext::beginGameViewRenderPass(VkCommandBuffer commandBuffer) {
    if (gameViewTarget.framebuffer == VK_NULL_HANDLE) return false;
    lveDevice.beginDebugLabel(commandBuffer, "Game View Render Pass", 0.25f, 0.8f, 0.35f);
    beginOffscreenRenderPass(commandBuffer, gameViewTarget);
    return true;
  }

  void RenderContext::endGameViewRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
    lveDevice.endDebugLabel(commandBuffer);
  }

  bool RenderContext::wasSwapChainRecreated() const {
    return swapChainRecreated;
  }

  void RenderContext::clearSwapChainRecreated() {
    swapChainRecreated = false;
  }

  void RenderContext::resetObjectDescriptorPools() {
    for (auto &pool : objectDescriptorPools) {
      if (pool) {
        pool->resetPool();
      }
    }
  }

  VkRenderPass RenderContext::getSwapChainRenderPass() const {
    return lveRenderer.getSwapChainRenderPass();
  }

  VkDescriptorSet RenderContext::getSceneViewDescriptor() const {
    return sceneViewTarget.imguiDescriptor;
  }

  VkDescriptorSet RenderContext::getGameViewDescriptor() const {
    return gameViewTarget.imguiDescriptor;
  }

  VkExtent2D RenderContext::getSceneViewExtent() const {
    return sceneViewTarget.extent;
  }

  VkExtent2D RenderContext::getGameViewExtent() const {
    return gameViewTarget.extent;
  }

  backend::RenderDebugStats RenderContext::getDebugStats() const {
    backend::RenderDebugStats stats{};
    stats.frameIndex = lveRenderer.getFrameindex();
    stats.swapChainImageCount = lveRenderer.getSwapChainImageCount();
    stats.sceneViewExtent = {sceneViewTarget.extent.width, sceneViewTarget.extent.height};
    stats.gameViewExtent = {gameViewTarget.extent.width, gameViewTarget.extent.height};
    stats.retiredOffscreenTargets = retiredOffscreenTargets.size();
    stats.simpleDescriptorCaches = descriptorCache.simpleObjectCacheCount();
    stats.spriteDescriptorCaches = descriptorCache.spriteObjectCacheCount();
    stats.subMeshDescriptorObjects = descriptorCache.subMeshObjectCacheCount();
    stats.subMeshDescriptorCaches = descriptorCache.subMeshCacheCount();
    stats.swapChainRecreated = swapChainRecreated;
    if (simpleRenderSystem) {
      stats.wireframeEnabled = simpleRenderSystem->isWireframeEnabled();
      stats.normalViewEnabled = simpleRenderSystem->isNormalView();
    }
    return stats;
  }

  FrameInfo RenderContext::makeFrameInfo(
    float frameTime,
    LveCamera &camera,
    std::vector<LveGameObject*> &gameObjects,
    VkCommandBuffer commandBuffer,
    int viewIndex) {
    int frameIndex = lveRenderer.getFrameindex();
    const int globalIndex = frameIndex * RENDER_VIEW_COUNT + viewIndex;
    return FrameInfo{
      frameIndex,
      frameTime,
      commandBuffer,
      camera,
      globalDescriptorSets[globalIndex],
      *objectDescriptorPools[frameIndex],
      descriptorCache,
      gameObjects};
  }

  void RenderContext::updateGlobalUbo(int frameIndex, int viewIndex, const GlobalUbo &ubo) {
    const int globalIndex = frameIndex * RENDER_VIEW_COUNT + viewIndex;
    uboBuffers[globalIndex]->writeToBuffer((void *)&ubo);
    uboBuffers[globalIndex]->flush();
  }

  void RenderContext::createOffscreenRenderPass() {
    offscreenColorFormat = lveRenderer.getSwapChainImageFormat();
    offscreenDepthFormat = lveDevice.findSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = offscreenDepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = offscreenColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(lveDevice.device(), &renderPassInfo, nullptr, &offscreenRenderPass) != VK_SUCCESS) {
      throw std::runtime_error("failed to create offscreen render pass");
    }
  }

  void RenderContext::destroyOffscreenRenderPass() {
    if (offscreenRenderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(lveDevice.device(), offscreenRenderPass, nullptr);
      offscreenRenderPass = VK_NULL_HANDLE;
    }
  }

  void RenderContext::destroyOffscreenTarget(OffscreenTarget &target) {
    if (target.imguiDescriptor != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(target.imguiDescriptor);
      target.imguiDescriptor = VK_NULL_HANDLE;
    }
    if (target.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(lveDevice.device(), target.sampler, nullptr);
      target.sampler = VK_NULL_HANDLE;
    }
    if (target.framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(lveDevice.device(), target.framebuffer, nullptr);
      target.framebuffer = VK_NULL_HANDLE;
    }
    if (target.colorView != VK_NULL_HANDLE) {
      vkDestroyImageView(lveDevice.device(), target.colorView, nullptr);
      target.colorView = VK_NULL_HANDLE;
    }
    if (target.colorImage != VK_NULL_HANDLE) {
      vkDestroyImage(lveDevice.device(), target.colorImage, nullptr);
      target.colorImage = VK_NULL_HANDLE;
    }
    if (target.colorMemory != VK_NULL_HANDLE) {
      vkFreeMemory(lveDevice.device(), target.colorMemory, nullptr);
      target.colorMemory = VK_NULL_HANDLE;
    }
    if (target.depthView != VK_NULL_HANDLE) {
      vkDestroyImageView(lveDevice.device(), target.depthView, nullptr);
      target.depthView = VK_NULL_HANDLE;
    }
    if (target.depthImage != VK_NULL_HANDLE) {
      vkDestroyImage(lveDevice.device(), target.depthImage, nullptr);
      target.depthImage = VK_NULL_HANDLE;
    }
    if (target.depthMemory != VK_NULL_HANDLE) {
      vkFreeMemory(lveDevice.device(), target.depthMemory, nullptr);
      target.depthMemory = VK_NULL_HANDLE;
    }
    target.extent = {};
  }

  void RenderContext::retireOffscreenTarget(OffscreenTarget &target) {
    const bool hasResources =
      target.imguiDescriptor != VK_NULL_HANDLE ||
      target.sampler != VK_NULL_HANDLE ||
      target.framebuffer != VK_NULL_HANDLE ||
      target.colorView != VK_NULL_HANDLE ||
      target.colorImage != VK_NULL_HANDLE ||
      target.depthView != VK_NULL_HANDLE ||
      target.depthImage != VK_NULL_HANDLE;
    if (!hasResources) {
      target.extent = {};
      return;
    }

    retiredOffscreenTargets.push_back(RetiredOffscreenTarget{target, 2});
    target = OffscreenTarget{};
  }

  void RenderContext::destroyRetiredOffscreenTargets() {
    for (auto &retired : retiredOffscreenTargets) {
      destroyOffscreenTarget(retired.target);
    }
    retiredOffscreenTargets.clear();
  }

  void RenderContext::collectRetiredOffscreenTargets() {
    for (auto it = retiredOffscreenTargets.begin(); it != retiredOffscreenTargets.end();) {
      it->framesRemaining--;
      if (it->framesRemaining <= 0) {
        destroyOffscreenTarget(it->target);
        it = retiredOffscreenTargets.erase(it);
      } else {
        ++it;
      }
    }
  }

  void RenderContext::createOffscreenTarget(OffscreenTarget &target, VkExtent2D extent, const char *debugName) {
    VkImageCreateInfo colorInfo{};
    colorInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorInfo.imageType = VK_IMAGE_TYPE_2D;
    colorInfo.extent.width = extent.width;
    colorInfo.extent.height = extent.height;
    colorInfo.extent.depth = 1;
    colorInfo.mipLevels = 1;
    colorInfo.arrayLayers = 1;
    colorInfo.format = offscreenColorFormat;
    colorInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    lveDevice.createImageWithInfo(
      colorInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      target.colorImage,
      target.colorMemory);
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.colorImage),
      VK_OBJECT_TYPE_IMAGE,
      debugName);

    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image = target.colorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = offscreenColorFormat;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(lveDevice.device(), &colorViewInfo, nullptr, &target.colorView) != VK_SUCCESS) {
      throw std::runtime_error("failed to create offscreen color view");
    }
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.colorView),
      VK_OBJECT_TYPE_IMAGE_VIEW,
      debugName);

    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.extent.width = extent.width;
    depthInfo.extent.height = extent.height;
    depthInfo.extent.depth = 1;
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.format = offscreenDepthFormat;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    lveDevice.createImageWithInfo(
      depthInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      target.depthImage,
      target.depthMemory);
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.depthImage),
      VK_OBJECT_TYPE_IMAGE,
      debugName);

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = target.depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = offscreenDepthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(lveDevice.device(), &depthViewInfo, nullptr, &target.depthView) != VK_SUCCESS) {
      throw std::runtime_error("failed to create offscreen depth view");
    }
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.depthView),
      VK_OBJECT_TYPE_IMAGE_VIEW,
      debugName);

    std::array<VkImageView, 2> attachments = {target.colorView, target.depthView};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = offscreenRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    if (vkCreateFramebuffer(lveDevice.device(), &framebufferInfo, nullptr, &target.framebuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to create offscreen framebuffer");
    }
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.framebuffer),
      VK_OBJECT_TYPE_FRAMEBUFFER,
      debugName);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &target.sampler) != VK_SUCCESS) {
      throw std::runtime_error("failed to create offscreen sampler");
    }
    lveDevice.setObjectName(
      reinterpret_cast<uint64_t>(target.sampler),
      VK_OBJECT_TYPE_SAMPLER,
      debugName);

    target.imguiDescriptor = ImGui_ImplVulkan_AddTexture(
      target.sampler,
      target.colorView,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    target.extent = extent;
  }

  void RenderContext::ensureOffscreenTargets(uint32_t sceneWidth, uint32_t sceneHeight, uint32_t gameWidth, uint32_t gameHeight) {
    const bool wantScene = sceneWidth > 0 && sceneHeight > 0;
    const bool wantGame = gameWidth > 0 && gameHeight > 0;
    const bool rebuildScene = wantScene &&
      (sceneViewTarget.extent.width != sceneWidth || sceneViewTarget.extent.height != sceneHeight);
    const bool rebuildGame = wantGame &&
      (gameViewTarget.extent.width != gameWidth || gameViewTarget.extent.height != gameHeight);
    const bool destroyScene = !wantScene && sceneViewTarget.framebuffer != VK_NULL_HANDLE;
    const bool destroyGame = !wantGame && gameViewTarget.framebuffer != VK_NULL_HANDLE;

    if (rebuildScene || destroyScene) {
      retireOffscreenTarget(sceneViewTarget);
      if (rebuildScene) {
        createOffscreenTarget(sceneViewTarget, {sceneWidth, sceneHeight}, "Scene View Offscreen Target");
      }
    }

    if (rebuildGame || destroyGame) {
      retireOffscreenTarget(gameViewTarget);
      if (rebuildGame) {
        createOffscreenTarget(gameViewTarget, {gameWidth, gameHeight}, "Game View Offscreen Target");
      }
    }
  }

} // namespace lve
