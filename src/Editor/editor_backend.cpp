#include "Editor/editor_backend.hpp"

#include "Engine/Backend/Vulkan/Editor/editor_render_backend.hpp"
#include "Engine/Backend/Vulkan/runtime_backend.hpp"

#include <stdexcept>
#include <utility>

namespace lve {

  EditorBackend::EditorBackend(
    std::unique_ptr<backend::RuntimeBackend> runtime,
    std::unique_ptr<backend::EditorRenderBackend> editorRender)
    : runtimeBackend{std::move(runtime)}
    , editorRenderBackend{std::move(editorRender)} {
    if (!runtimeBackend || !editorRenderBackend) {
      throw std::runtime_error("EditorBackend requires runtime and editor render backends.");
    }
  }

  EditorBackend createEditorBackend(const backend::RuntimeBackendConfig &config) {
    switch (config.api) {
      case backend::BackendApi::Vulkan: {
        auto runtime = std::make_unique<backend::VulkanRuntimeBackend>(
          config.width,
          config.height,
          config.title);
        auto *vulkanRuntime = runtime.get();
        auto editorRenderBackend = std::make_unique<backend::VulkanEditorRenderBackend>(
          vulkanRuntime->nativeWindow(),
          vulkanRuntime->vulkanDevice());
        return EditorBackend{std::move(runtime), std::move(editorRenderBackend)};
      }
      default:
        throw std::runtime_error("Unsupported editor backend API.");
    }
  }

} // namespace lve
