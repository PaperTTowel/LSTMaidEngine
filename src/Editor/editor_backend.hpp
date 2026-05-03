#pragma once

#include "Engine/Backend/Factory/runtime_backend_factory.hpp"
#include "Engine/Backend/editor_render_backend.hpp"
#include "Engine/Backend/runtime_backend.hpp"

#include <memory>

namespace lve {

  class EditorBackend {
  public:
    EditorBackend(
      std::unique_ptr<backend::RuntimeBackend> runtime,
      std::unique_ptr<backend::EditorRenderBackend> editorRenderBackend);

    EditorBackend(const EditorBackend &) = delete;
    EditorBackend &operator=(const EditorBackend &) = delete;
    EditorBackend(EditorBackend &&) = default;
    EditorBackend &operator=(EditorBackend &&) = default;

    backend::RuntimeBackend &runtime() { return *runtimeBackend; }
    backend::EditorRenderBackend &editorRender() { return *editorRenderBackend; }

  private:
    std::unique_ptr<backend::RuntimeBackend> runtimeBackend;
    std::unique_ptr<backend::EditorRenderBackend> editorRenderBackend;
  };

  EditorBackend createEditorBackend(const backend::RuntimeBackendConfig &config);

} // namespace lve
