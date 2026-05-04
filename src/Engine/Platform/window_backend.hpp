#pragma once

#include "Engine/Rendering/runtime_window.hpp"
#include "Engine/Platform/glfw_input.hpp"

namespace lve::backend {
  class GlfwWindowBackend final : public WindowBackend {
  public:
    GlfwWindowBackend(LveWindow &window, GlfwInputProvider &inputProvider)
      : window{window}, inputProvider{inputProvider} {}

    void pollEvents() override;
    bool shouldClose() const override;
    RenderExtent getExtent() const override;
    InputProvider &input() override { return inputProvider; }
    const InputProvider &input() const override { return inputProvider; }

  private:
    LveWindow &window;
    GlfwInputProvider &inputProvider;
  };
} // namespace lve::backend
