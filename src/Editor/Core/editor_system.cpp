#include "editor_system.hpp"

#include "Engine/Scene/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

#include <string>
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

  void EditorSystem::markSceneDirty(const char *reason) {
    scenePanelState.dirty = true;
    scenePanelState.statusMessage = reason ? reason : "Modified";
  }

  void EditorSystem::markSceneClean(const std::string &path, const char *status) {
    scenePanelState.currentPath = path;
    scenePanelState.path = path;
    scenePanelState.dirty = false;
    scenePanelState.statusMessage = status ? status : "Ready";
  }

  void EditorSystem::requestSceneLoad(EditorFrameResult &result) {
    if (scenePanelState.dirty) {
      scenePanelState.loadConfirmRequested = true;
      return;
    }
    result.sceneActions.loadRequested = true;
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
    if (hierarchyState.selectedId) {
      if (auto *selected = sceneSystem.findObject(*hierarchyState.selectedId)) {
        result.selectedSnapshotBeforeUi = editor::CaptureSnapshot(*selected);
      }
    }
    if (auto *activeCamera = sceneSystem.findActiveCamera()) {
      result.activeCameraBeforeUi = activeCamera->getId();
    }

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
