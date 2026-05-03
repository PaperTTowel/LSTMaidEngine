#pragma once

#include "Engine/Backend/runtime_backend.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Window/glfw_input.hpp"
#include "Engine/Backend/Window/window_backend.hpp"
#include "Engine/Backend/Vulkan/Render/asset_factory.hpp"
#include "Engine/Backend/Vulkan/Render/render_backend.hpp"
#include "Engine/scene_system.hpp"

#include <memory>
#include <string>

namespace lve::backend {
  class VulkanRuntimeBackend final : public RuntimeBackend {
  public:
    VulkanRuntimeBackend(int width, int height, std::string title);

    WindowBackend &window() override { return windowBackend; }
    RenderBackend &renderBackend() override { return renderBackendImpl; }
    SceneSystem &sceneSystem() override { return sceneSystemImpl; }
    LveWindow &nativeWindow() { return windowImpl; }
    LveDevice &vulkanDevice() { return device; }

  private:
    LveWindow windowImpl;
    GlfwInputProvider inputProvider;
    GlfwWindowBackend windowBackend;
    LveDevice device;
    VulkanRenderAssetFactory assetFactory;
    SceneSystem sceneSystemImpl;
    VulkanRenderBackend renderBackendImpl;
  };
} // namespace lve::backend
