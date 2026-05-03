#pragma once

#include "Engine/Backend/runtime_backend.hpp"

// std
#include <memory>

namespace lve {
  class EngineLoop {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    explicit EngineLoop(std::unique_ptr<backend::RuntimeBackend> runtime);
    ~EngineLoop();

    EngineLoop(const EngineLoop &) = delete;
    EngineLoop &operator=(const EngineLoop &) = delete;

    backend::RuntimeBackend &getRuntime() { return *runtime; }
    const backend::RuntimeBackend &getRuntime() const { return *runtime; }

  private:
    std::unique_ptr<backend::RuntimeBackend> runtime;
  };
} // namespace lve



