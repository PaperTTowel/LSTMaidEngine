#include "Backends/Vulkan/Core/window_surface.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace lve {

  std::vector<const char *> VulkanWindowSurface::getRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (!glfwExtensions || glfwExtensionCount == 0) {
      return {};
    }
    return std::vector<const char *>(glfwExtensions, glfwExtensions + glfwExtensionCount);
  }

  void VulkanWindowSurface::create(LveWindow &window, VkInstance instance, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, window.getGLFWwindow(), nullptr, surface) != VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
  }

} // namespace lve
