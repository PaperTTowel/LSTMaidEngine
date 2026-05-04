#pragma once

#include "Engine/Platform/window.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace lve {

  class VulkanWindowSurface {
  public:
    static std::vector<const char *> getRequiredInstanceExtensions();
    static void create(LveWindow &window, VkInstance instance, VkSurfaceKHR *surface);
  };

} // namespace lve
