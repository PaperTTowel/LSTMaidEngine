#include "editor_system.hpp"

#include "Engine/Scene/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

#include <vector>

namespace lve {

  EditorSystem::EditorSystem(backend::EditorRenderBackend &renderBackend)
    : renderBackend{renderBackend} {
    gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
    gizmoMode = static_cast<int>(ImGuizmo::LOCAL);
  }

  void EditorSystem::init(backend::RenderPassHandle renderPass, std::uint32_t imageCount) {
    renderBackend.init(renderPass, imageCount);
  }

  void EditorSystem::onRenderPassChanged(backend::RenderPassHandle renderPass, std::uint32_t imageCount) {
    renderBackend.onRenderPassChanged(renderPass, imageCount);
  }

  void EditorSystem::shutdown() {
    renderBackend.shutdown();
  }

  EditorFrameResult EditorSystem::update(
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    SceneSystem &sceneSystem,
    LveGameObject::id_t protectedId,
    LveGameObject::id_t viewerId,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    backend::RenderExtent viewportExtent,
    const backend::RenderDebugStats &renderDebugStats,
    editor::ResourceBrowserState &resourceBrowserState,
    void *sceneViewTextureId,
    void *gameViewTextureId) {

    EditorFrameResult result{};

    std::vector<LveGameObject*> objects;
    sceneSystem.collectObjects(objects);

    buildFrameUI(
      result,
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      sceneSystem,
      objects,
      protectedId,
      animator,
      view,
      projection,
      viewportExtent,
      renderDebugStats,
      resourceBrowserState,
      sceneViewTextureId,
      gameViewTextureId);

    const bool historyTriggered = applyHistoryActions(result, sceneSystem);

    applyResourceActions(result, sceneSystem, animator, resourceBrowserState);
    applyInspectorActions(result, sceneSystem, animator, resourceBrowserState);
    handlePicking(result, objects, view, projection);
    handleCreateDelete(
      result,
      sceneSystem,
      animator,
      view,
      cameraPos,
      resourceBrowserState,
      protectedId,
      historyTriggered);
    handleSceneActions(result, sceneSystem, animator, viewerId);

    return result;
  }

  void EditorSystem::render(backend::CommandBufferHandle commandBuffer) {
    renderBackend.render(commandBuffer);
  }

  void EditorSystem::renderPlatformWindows() {
    renderBackend.renderPlatformWindows();
  }

} // namespace lve
