#pragma once

#include <cstddef>
#include <cstdint>

namespace lve::backend {
  constexpr int kMaxFramesInFlight = 2;

  struct RenderExtent {
    std::uint32_t width{0};
    std::uint32_t height{0};
  };

  struct BufferInfo {
    std::uintptr_t buffer{0};
    std::uint64_t offset{0};
    std::uint64_t range{0};
  };

  struct RenderDebugStats {
    int frameIndex{0};
    std::size_t swapChainImageCount{0};
    RenderExtent sceneViewExtent{};
    RenderExtent gameViewExtent{};
    std::size_t retiredOffscreenTargets{0};
    std::size_t simpleDescriptorCaches{0};
    std::size_t spriteDescriptorCaches{0};
    std::size_t subMeshDescriptorObjects{0};
    std::size_t subMeshDescriptorCaches{0};
    bool swapChainRecreated{false};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
  };

  using RenderPassHandle = void *;
  using CommandBufferHandle = void *;
  using DescriptorSetHandle = void *;
} // namespace lve::backend
