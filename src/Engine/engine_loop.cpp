#include "engine_loop.hpp"

// std
#include <stdexcept>
#include <utility>

namespace lve {

  EngineLoop::EngineLoop(std::unique_ptr<backend::RuntimeBackend> runtimeBackend)
    : runtime{std::move(runtimeBackend)} {
    if (!runtime) {
      throw std::runtime_error("EngineLoop requires a runtime backend.");
    }
  }

  EngineLoop::~EngineLoop() {}

} // namespace lve
