#include "Engine/Backend/Window/window.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <utility>

namespace lve {

  LveWindow::LveWindow(const WindowConfig &config)
    : width{config.width}
    , height{config.height}
    , clientApi{config.clientApi}
    , windowName{config.title} {
    initWindow();
  }

  LveWindow::LveWindow(int w, int h, std::string name, WindowClientApi clientApi)
    : width{w}
    , height{h}
    , clientApi{clientApi}
    , windowName{std::move(name)} {
    initWindow();
  }

  LveWindow::~LveWindow() {
    if (window) {
      glfwDestroyWindow(window);
      window = nullptr;
    }
    glfwTerminate();
  }

  void LveWindow::initWindow() {
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW");
    }

    if (clientApi == WindowClientApi::Vulkan) {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } else {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    if (!window) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  bool LveWindow::shouldClose() const {
    return glfwWindowShouldClose(window);
  }

  void LveWindow::pollEvents() { glfwPollEvents(); }

  void LveWindow::waitEvents() { glfwWaitEvents(); }

  void LveWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto lveWindow = reinterpret_cast<LveWindow *>(glfwGetWindowUserPointer(window));
    lveWindow->framebufferResized = true;
    lveWindow->width = width;
    lveWindow->height = height;
  }

} // namespace lve
