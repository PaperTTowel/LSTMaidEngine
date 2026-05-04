#include "Editor/Core/editor_system.hpp"

#include "Editor/Workflow/editor_import.hpp"
#include "Engine/IO/material_io.hpp"
#include "Engine/Scene/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace lve {
  namespace {
    const char *kGameViewCameraWarning = u8"\uC9C0\uC815 Game View \uCE74\uBA54\uB77C\uAC00 \uC0DD\uC131\uB418\uC9C0 \uC54A\uC558\uC2B5\uB2C8\uB2E4.\n"
      u8"\uCE90\uB9AD\uD130 \uACE0\uC815\uC2DC\uC810 \uCE74\uBA54\uB77C\uB85C \uC790\uB3D9\uC73C\uB85C \uC720\uC9C0\uB429\uB2C8\uB2E4";

    void DrawSceneLoadConfirm(editor::ScenePanelState &state, EditorFrameResult &result) {
      if (state.loadConfirmRequested) {
        ImGui::OpenPopup("Unsaved Scene Changes");
        state.loadConfirmRequested = false;
        state.loadConfirmOpen = true;
      }

      if (ImGui::BeginPopupModal(
            "Unsaved Scene Changes",
            &state.loadConfirmOpen,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Current scene has unsaved changes.");
        ImGui::TextWrapped("Load %s and discard them?", state.path.c_str());
        ImGui::Separator();
        if (ImGui::Button("Load")) {
          result.sceneActions.loadRequested = true;
          state.loadConfirmOpen = false;
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          state.loadConfirmOpen = false;
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }

  } // namespace

  namespace fs = std::filesystem;
  void EditorSystem::buildFrameUI(
    EditorFrameResult &result,
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    SceneSystem &sceneSystem,
    const std::vector<LveGameObject*> &objects,
    LveGameObject::id_t protectedId,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    backend::RenderExtent viewportExtent,
    const backend::RenderDebugStats &renderDebugStats,
    editor::ResourceBrowserState &resourceBrowserState,
    void *sceneViewTextureId,
    void *gameViewTextureId) {

    renderBackend.newFrame();
    renderBackend.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      showEngineStats);
    const bool showCameraWarning =
      showGameViewCameraWarning && sceneSystem.findActiveCamera() == nullptr;

    ImGuiIO &io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift) {
        result.redoRequested = true;
      } else {
        result.undoRequested = true;
      }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
      result.redoRequested = true;
    }

    editor::HierarchyCreateRequest menuCreateRequest = editor::HierarchyCreateRequest::None;
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Scene")) {
          result.sceneActions.saveRequested = true;
        }
        if (ImGui::MenuItem("Load Scene")) {
          requestSceneLoad(result);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = history.canUndo();
        const bool canRedo = history.canRedo();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
          result.undoRequested = true;
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
          result.redoRequested = true;
        }
        ImGui::MenuItem("Undo History", nullptr, &showUndoHistory);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::BeginMenu("Create Object")) {
          if (ImGui::MenuItem("Camera")) {
            menuCreateRequest = editor::HierarchyCreateRequest::Camera;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("2D Object")) {
          if (ImGui::MenuItem("Sprite")) {
            menuCreateRequest = editor::HierarchyCreateRequest::Sprite;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("3D Object")) {
          if (ImGui::MenuItem("Mesh")) {
            menuCreateRequest = editor::HierarchyCreateRequest::Mesh;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
          if (ImGui::MenuItem("Point Light")) {
            menuCreateRequest = editor::HierarchyCreateRequest::PointLight;
          }
          ImGui::EndMenu();
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Window")) {
        if (ImGui::BeginMenu("Panels")) {
          ImGui::MenuItem("Engine Stats", nullptr, &showEngineStats);
          ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
          ImGui::MenuItem("Inspector", nullptr, &showInspector);
          ImGui::MenuItem("Resource Browser", nullptr, &showResourceBrowser);
          ImGui::MenuItem("Scene View", nullptr, &showSceneView);
          ImGui::MenuItem("Game View", nullptr, &showGameView);
          ImGui::MenuItem("Renderer Debug", nullptr, &showRendererDebug);
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
          ImGui::MenuItem("Scene", nullptr, &showScene);
          ImGui::MenuItem("Game View Camera Warning", nullptr, &showGameViewCameraWarning);
          ImGui::EndMenu();
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About Engine")) {
          showAboutEnginePopup = true;
        }
        if (ImGui::MenuItem("Report a bug")) {
          showReportBugPopup = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    if (showAboutEnginePopup) {
      ImGui::OpenPopup("About Engine");
    }
    if (ImGui::BeginPopupModal("About Engine", &showAboutEnginePopup, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped("PaperTTowel Engine\n\n"
        "This Engine is for studying using imgui and vulkan\n\nwill add openGL support later\n\n"
        "Copyright (c) 2025 PaperTTowel");
      ImGui::Dummy(ImVec2(360.f, 140.f));
      ImGui::Separator();
      if (ImGui::Button("OK")) {
        showAboutEnginePopup = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    if (showReportBugPopup) {
      ImGui::OpenPopup("Report a bug");
    }
    if (ImGui::BeginPopupModal("Report a bug", &showReportBugPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped("To report a bug, please visit my GitHub page or email me at \'mycat210117@icloud.com\'");
      ImGui::Dummy(ImVec2(360.f, 140.f));
      ImGui::Separator();
      if (ImGui::Button("OK")) {
        showReportBugPopup = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    editor::GizmoContext gizmoContext{};
    if (showSceneView) {
      if (ImGui::Begin("Scene View", &showSceneView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::RadioButton("Move", gizmoOperation == static_cast<int>(ImGuizmo::TRANSLATE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", gizmoOperation == static_cast<int>(ImGuizmo::ROTATE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::ROTATE);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", gizmoOperation == static_cast<int>(ImGuizmo::SCALE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::SCALE);
        }
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        ImGui::Checkbox("Grid", &showSceneGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &gizmoSnap.enabled);
        if (gizmoSnap.enabled) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.f);
          if (gizmoOperation == static_cast<int>(ImGuizmo::ROTATE)) {
            ImGui::DragFloat("##SnapRotate", &gizmoSnap.rotate, 1.f, 1.f, 90.f, "%.0f deg");
          } else if (gizmoOperation == static_cast<int>(ImGuizmo::SCALE)) {
            ImGui::DragFloat("##SnapScale", &gizmoSnap.scale, 0.01f, 0.01f, 10.f, "%.2f");
          } else {
            ImGui::DragFloat("##SnapTranslate", &gizmoSnap.translate, 0.05f, 0.01f, 100.f, "%.2f");
          }
        }
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", gizmoMode == static_cast<int>(ImGuizmo::LOCAL))) {
          gizmoMode = static_cast<int>(ImGuizmo::LOCAL);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("World", gizmoMode == static_cast<int>(ImGuizmo::WORLD))) {
          gizmoMode = static_cast<int>(ImGuizmo::WORLD);
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        result.sceneView.width = static_cast<uint32_t>(avail.x > 0 ? avail.x : 0);
        result.sceneView.height = static_cast<uint32_t>(avail.y > 0 ? avail.y : 0);
        result.sceneView.visible = true;
        result.sceneView.x = contentPos.x;
        result.sceneView.y = contentPos.y;
        result.sceneView.hovered = ImGui::IsWindowHovered();
        ImGuiIO &io = ImGui::GetIO();
        result.sceneView.rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        result.sceneView.leftMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        result.sceneView.mouseDeltaX = io.MouseDelta.x;
        result.sceneView.mouseDeltaY = io.MouseDelta.y;
        result.sceneView.mousePosX = io.MousePos.x;
        result.sceneView.mousePosY = io.MousePos.y;
        result.sceneView.allowPick = result.sceneView.hovered && !ImGuizmo::IsOver();
        gizmoContext.drawList = ImGui::GetWindowDrawList();
        gizmoContext.x = contentPos.x;
        gizmoContext.y = contentPos.y;
        gizmoContext.width = avail.x;
        gizmoContext.height = avail.y;
        gizmoContext.valid = (avail.x > 0 && avail.y > 0);
        if (result.sceneView.hovered &&
            !io.WantTextInput &&
            !io.WantCaptureKeyboard &&
            ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
          if (io.KeyShift) {
            if (gizmoOperation == static_cast<int>(ImGuizmo::TRANSLATE)) {
              gizmoOperation = static_cast<int>(ImGuizmo::SCALE);
            } else if (gizmoOperation == static_cast<int>(ImGuizmo::SCALE)) {
              gizmoOperation = static_cast<int>(ImGuizmo::ROTATE);
            } else {
              gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
            }
          } else {
            if (gizmoOperation == static_cast<int>(ImGuizmo::TRANSLATE)) {
              gizmoOperation = static_cast<int>(ImGuizmo::ROTATE);
            } else if (gizmoOperation == static_cast<int>(ImGuizmo::ROTATE)) {
              gizmoOperation = static_cast<int>(ImGuizmo::SCALE);
            } else {
              gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
            }
          }
        }
        if (result.sceneView.hovered &&
            !io.WantTextInput &&
            !io.WantCaptureKeyboard &&
            ImGui::IsKeyPressed(ImGuiKey_F, false)) {
          result.focusSelectedRequested = true;
        }
        if (result.sceneView.hovered &&
            !io.WantTextInput &&
            !io.WantCaptureKeyboard &&
            io.KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_D, false)) {
          result.duplicateSelectedRequested = true;
        }
        if (sceneViewTextureId && result.sceneView.width > 0 && result.sceneView.height > 0) {
          ImGui::Image(sceneViewTextureId, avail);
        } else {
          ImGui::TextUnformatted("Scene view not ready");
        }
        result.sceneGridEnabled = showSceneGrid;

        if (gizmoContext.valid && gizmoContext.drawList) {
          ImDrawList *drawList = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
          const float gizmoSize = 70.0f;
          const float margin = 10.0f;
          const ImVec2 center{
            gizmoContext.x + gizmoContext.width - margin - gizmoSize * 0.5f,
            gizmoContext.y + margin + gizmoSize * 0.5f};

          const glm::vec3 axisX = glm::normalize(glm::vec3(view * glm::vec4(1.f, 0.f, 0.f, 0.f)));
          const glm::vec3 axisY = glm::normalize(glm::vec3(view * glm::vec4(0.f, 1.f, 0.f, 0.f)));
          const glm::vec3 axisZ = glm::normalize(glm::vec3(view * glm::vec4(0.f, 0.f, 1.f, 0.f)));

          auto drawAxis = [&](const glm::vec3 &axis, ImU32 color, const char *label) {
            ImVec2 end{
              center.x + axis.x * (gizmoSize * 0.45f),
              center.y - axis.y * (gizmoSize * 0.45f)};
            drawList->AddLine(center, end, color, 2.0f);
            drawList->AddText(
              ImVec2(end.x + 4.0f, end.y + 2.0f),
              color,
              label);
          };

          drawAxis(axisX, IM_COL32(220, 60, 60, 255), "X");
          drawAxis(axisY, IM_COL32(60, 220, 60, 255), "Y");
          drawAxis(axisZ, IM_COL32(60, 120, 220, 255), "Z");
        }
      }
      ImGui::End();
    }

    if (showGameView) {
      if (ImGui::Begin("Game View", &showGameView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        result.gameView.width = static_cast<uint32_t>(avail.x > 0 ? avail.x : 0);
        result.gameView.height = static_cast<uint32_t>(avail.y > 0 ? avail.y : 0);
        result.gameView.visible = true;
        result.gameView.x = contentPos.x;
        result.gameView.y = contentPos.y;
        result.gameView.hovered = ImGui::IsWindowHovered();
        if (gameViewTextureId && result.gameView.width > 0 && result.gameView.height > 0) {
          ImGui::Image(gameViewTextureId, avail);
        } else {
          ImGui::TextUnformatted("Game view not ready");
        }
        if (showCameraWarning) {
          ImDrawList *drawList = ImGui::GetWindowDrawList();
          ImVec2 textSize = ImGui::CalcTextSize(kGameViewCameraWarning);
          const ImVec2 padding{8.f, 6.f};
          const ImVec2 origin{contentPos.x + 12.f, contentPos.y + 12.f};
          const ImVec2 bgMin{origin.x, origin.y};
          const ImVec2 bgMax{origin.x + textSize.x + padding.x * 2.f, origin.y + textSize.y + padding.y * 2.f};
          drawList->AddRectFilled(bgMin, bgMax, IM_COL32(20, 20, 20, 200), 4.f);
          drawList->AddRect(bgMin, bgMax, IM_COL32(255, 200, 120, 200), 4.f);
          drawList->AddText(
            ImVec2(origin.x + padding.x, origin.y + padding.y),
            IM_COL32(255, 200, 120, 255),
            kGameViewCameraWarning);
        }
      }
      ImGui::End();
    }

    if (showRendererDebug) {
      if (ImGui::Begin("Renderer Debug", &showRendererDebug)) {
        ImGui::Text("Frame index: %d", renderDebugStats.frameIndex);
        ImGui::Text("Swapchain images: %zu", renderDebugStats.swapChainImageCount);
        ImGui::Separator();
        ImGui::Text(
          "Scene view: %u x %u",
          renderDebugStats.sceneViewExtent.width,
          renderDebugStats.sceneViewExtent.height);
        ImGui::Text(
          "Game view: %u x %u",
          renderDebugStats.gameViewExtent.width,
          renderDebugStats.gameViewExtent.height);
        ImGui::Separator();
        ImGui::Text("Retired offscreen targets: %zu", renderDebugStats.retiredOffscreenTargets);
        ImGui::Text("Simple descriptor caches: %zu", renderDebugStats.simpleDescriptorCaches);
        ImGui::Text("Sprite descriptor caches: %zu", renderDebugStats.spriteDescriptorCaches);
        ImGui::Text("Submesh descriptor objects: %zu", renderDebugStats.subMeshDescriptorObjects);
        ImGui::Text("Submesh descriptor caches: %zu", renderDebugStats.subMeshDescriptorCaches);
        ImGui::Separator();
        ImGui::Text("Wireframe: %s", renderDebugStats.wireframeEnabled ? "on" : "off");
        ImGui::Text("Normal view: %s", renderDebugStats.normalViewEnabled ? "on" : "off");
        ImGui::Text("Swapchain recreated: %s", renderDebugStats.swapChainRecreated ? "yes" : "no");
      }
      ImGui::End();
    }

    if (showHierarchy) {
      result.hierarchyActions = editor::BuildHierarchyPanel(
        objects,
        hierarchyState,
        protectedId,
        &showHierarchy);
    }
    if (menuCreateRequest != editor::HierarchyCreateRequest::None) {
      result.hierarchyActions.createRequest = menuCreateRequest;
    }
    if (showUndoHistory) {
      if (ImGui::Begin("Undo History", &showUndoHistory)) {
        const auto &commands = history.getCommands();
        const std::size_t cursor = history.getCursor();
        std::size_t targetCursor = cursor;
        bool selectionChanged = false;

        ImGui::PushID("undo_start");
        if (ImGui::Selectable("Start", cursor == 0)) {
          targetCursor = 0;
          selectionChanged = true;
        }
        ImGui::PopID();
        for (std::size_t i = 0; i < commands.size(); ++i) {
          std::string label = commands[i].label.empty() ? "Action" : commands[i].label;
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::Selectable(label.c_str(), (i + 1) == cursor)) {
            targetCursor = i + 1;
            selectionChanged = true;
          }
          ImGui::PopID();
        }

        if (selectionChanged && targetCursor != cursor) {
          if (targetCursor < cursor) {
            result.undoSteps += static_cast<int>(cursor - targetCursor);
          } else {
            result.redoSteps += static_cast<int>(targetCursor - cursor);
          }
        }
      }
      ImGui::End();
    }

    if (showScene) {
      auto sceneActions = editor::BuildScenePanel(scenePanelState, &showScene);
      result.sceneActions.saveRequested |= sceneActions.saveRequested;
      if (sceneActions.loadRequested) {
        requestSceneLoad(result);
      }
    }

    DrawSceneLoadConfirm(scenePanelState, result);

    if (hierarchyState.selectedId) {
      result.selectedObject = sceneSystem.findObject(*hierarchyState.selectedId);
    }

    if (showInspector) {
      const editor::MaterialPickResult materialPick = pendingMaterialPick;
      result.inspectorActions = editor::BuildInspectorPanel(
        result.selectedObject,
        animator,
        inspectorState,
        renderBackend,
        view,
        projection,
        viewportExtent,
        &showInspector,
        gizmoContext,
        gizmoSnap,
        gizmoOperation,
        gizmoMode,
        hierarchyState.selectedNodeIndex,
        materialPick);
      if (materialPick.available) {
        pendingMaterialPick.available = false;
      }
    }

    if (showResourceBrowser) {
      result.resourceActions = editor::BuildResourceBrowserPanel(
        resourceBrowserState,
        result.selectedObject,
        &showResourceBrowser);
    }

    if (showFileDialog) {
      result.fileDialogActions = editor::BuildFileDialogPanel(fileDialogState, &showFileDialog);
    }

    if (result.fileDialogActions.accepted) {
      if (fileDialogPurpose == FileDialogPurpose::Import) {
        importOptions.pendingPath = result.fileDialogActions.selectedPath;
        importOptions.error.clear();
        importOptions.mode = 0;
        importOptions.show = true;
        importOptions.openRequested = true;
      } else if (fileDialogPurpose == FileDialogPurpose::MaterialTexture) {
        const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
          ? "Assets"
          : resourceBrowserState.browser.rootPath;
        pendingMaterialPick.available = true;
        pendingMaterialPick.slot = pendingMaterialPickSlot;
        pendingMaterialPick.path = editor::workflow::ToAssetPath(result.fileDialogActions.selectedPath, rootPath);
      }
      fileDialogPurpose = FileDialogPurpose::Import;
    }

    if (importOptions.openRequested) {
      ImGui::OpenPopup("Import Options");
      importOptions.openRequested = false;
    }

    if (importOptions.show) {
      bool popupOpen = true;
      if (ImGui::BeginPopupModal("Import Options", &popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Source: %s", importOptions.pendingPath.c_str());
        ImGui::Separator();
        if (ImGui::RadioButton("Link external file", importOptions.mode == 0)) {
          importOptions.mode = 0;
        }
        if (ImGui::RadioButton("Copy into Assets", importOptions.mode == 1)) {
          importOptions.mode = 1;
        }

        std::string previewTarget = "-";
        const fs::path srcPath = importOptions.pendingPath;
        const std::string root = resourceBrowserState.browser.rootPath.empty()
          ? "Assets"
          : resourceBrowserState.browser.rootPath;
        if (!importOptions.pendingPath.empty()) {
          fs::path targetDir = fs::path(root) / "links";
          if (importOptions.mode == 1) {
            targetDir = fs::path(root) / editor::workflow::PickImportSubdir(srcPath);
          }
          previewTarget = (targetDir / srcPath.filename()).generic_string();
        }

        ImGui::Spacing();
        ImGui::Text("Target: %s", previewTarget.c_str());

        if (!importOptions.error.empty()) {
          ImGui::Spacing();
          ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "Error: %s", importOptions.error.c_str());
        }

        ImGui::Spacing();
        bool doImport = false;
        if (ImGui::Button("Import")) {
          doImport = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          popupOpen = false;
          ImGui::CloseCurrentPopup();
        }

        if (doImport) {
          importOptions.error.clear();
          const fs::path sourcePath = importOptions.pendingPath;
          std::string finalPath = sourcePath.generic_string();
          fs::path importedPath;

          if (importOptions.mode == 1) {
            if (!editor::workflow::CopyIntoAssets(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath);
            }
          } else {
            if (!editor::workflow::CreateLinkStub(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath, sourcePath.generic_string());
            }
          }

          if (doImport) {
            if (editor::workflow::IsMeshFile(finalPath)) {
              resourceBrowserState.activeMeshPath = finalPath;
            } else if (editor::workflow::IsSpriteMetaFile(finalPath)) {
              resourceBrowserState.activeSpriteMetaPath = finalPath;
            } else if (editor::workflow::IsMaterialFile(finalPath)) {
              resourceBrowserState.activeMaterialPath = finalPath;
            }
            popupOpen = false;
            ImGui::CloseCurrentPopup();
          }
        }

        ImGui::EndPopup();
      }
      if (!popupOpen) {
        importOptions.show = false;
      }
    }
  }


} // namespace lve
