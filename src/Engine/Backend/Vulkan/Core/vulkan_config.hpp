#pragma once

#include "Engine/Backend/render_types.hpp"

#include <vulkan/vulkan.h>

namespace lve {

  constexpr uint32_t kVulkanApiVersion = VK_API_VERSION_1_0;
  constexpr int kVulkanMaxFramesInFlight = backend::kMaxFramesInFlight;

} // namespace lve
