#include "Editor/editor_frame_controller.hpp"

#include "Engine/scene_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace lve {

  void EditorFrameController::initialize(
    SceneSystem &sceneSystem,
    backend::WindowBackend &window) {
    auto &viewerObject = sceneSystem.createEmptyObject();
    viewerObject.transform.translation.z = -2.5f;
    viewerId = viewerObject.getId();

    const backend::RenderExtent initialExtent = window.getExtent();
    sceneViewInfo.width = initialExtent.width;
    sceneViewInfo.height = initialExtent.height;
    sceneViewInfo.visible = true;
    gameViewInfo = sceneViewInfo;

    const auto &defaults = sceneSystem.getAssetDefaults();
    resourceBrowserState.browser.rootPath = defaults.rootPath;
    resourceBrowserState.browser.currentPath = defaults.rootPath;
    resourceBrowserState.browser.pendingRefresh = true;
    resourceBrowserState.activeMeshPath = defaults.activeMeshPath;
    resourceBrowserState.activeSpriteMetaPath = defaults.activeSpriteMetaPath;
    resourceBrowserState.activeMaterialPath = defaults.activeMaterialPath;
    initialized = true;
  }

  EditorFrameState EditorFrameController::update(
    float frameTime,
    backend::InputProvider &input,
    backend::WindowBackend &window,
    backend::RenderBackend &renderBackend,
    EditorSystem &editorSystem,
    SceneSystem &sceneSystem,
    LveGameObject::id_t characterId,
    SpriteAnimator *&spriteAnimator) {
    if (!initialized) {
      initialize(sceneSystem, window);
    }

    auto *viewerObject = sceneSystem.findObject(viewerId);
    if (!viewerObject) {
      auto &newViewer = sceneSystem.createEmptyObject();
      newViewer.transform.translation.z = -2.5f;
      viewerId = newViewer.getId();
      viewerObject = &newViewer;
    }

    if (sceneViewInfo.hovered) {
      cameraController.moveInPlaneXZ(input, frameTime, *viewerObject);
    }
    if (sceneViewInfo.hovered && sceneViewInfo.rightMouseDown) {
      const float mouseSensitivity = 0.003f;
      viewerObject->transform.rotation.y += sceneViewInfo.mouseDeltaX * mouseSensitivity;
      viewerObject->transform.rotation.x -= sceneViewInfo.mouseDeltaY * mouseSensitivity;
    }
    viewerObject->transformDirty = true;
    viewerObject->transform.rotation.x = glm::clamp(viewerObject->transform.rotation.x, -1.5f, 1.5f);
    viewerObject->transform.rotation.y = glm::mod(viewerObject->transform.rotation.y, glm::two_pi<float>());
    editorCamera.setViewYXZ(viewerObject->transform.translation, viewerObject->transform.rotation);

    const backend::RenderExtent windowExtent = window.getExtent();
    const uint32_t sceneWidth = sceneViewInfo.width > 0 ? sceneViewInfo.width : windowExtent.width;
    const uint32_t sceneHeight = sceneViewInfo.height > 0 ? sceneViewInfo.height : windowExtent.height;
    const float sceneAspect = sceneHeight > 0
      ? (static_cast<float>(sceneWidth) / static_cast<float>(sceneHeight))
      : renderBackend.getAspectRatio();
    editorCamera.setPerspectiveProjection(glm::radians(50.f), sceneAspect, 0.1f, 100.f);

    EditorFrameResult editorResult = editorSystem.update(
      frameTime,
      viewerObject->transform.translation,
      viewerObject->transform.rotation,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      sceneSystem,
      characterId,
      viewerId,
      spriteAnimator,
      editorCamera.getView(),
      editorCamera.getProjection(),
      backend::RenderExtent{sceneWidth, sceneHeight},
      renderBackend.getDebugStats(),
      resourceBrowserState,
      renderBackend.getSceneViewDescriptor(),
      renderBackend.getGameViewDescriptor());

    sceneViewInfo = editorResult.sceneView;
    gameViewInfo = editorResult.gameView;

    renderBackend.setWireframe(wireframeEnabled);
    renderBackend.setNormalView(normalViewEnabled);

    return EditorFrameState{
      editorResult,
      editorCamera,
      sceneViewInfo,
      gameViewInfo,
      useOrthoCamera};
  }

} // namespace lve
