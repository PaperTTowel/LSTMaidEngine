#include "Editor/Core/editor_system.hpp"

#include "Editor/History/editor_snapshot.hpp"
#include "Editor/Tools/editor_picking.hpp"
#include "Editor/Workflow/editor_import.hpp"
#include "Engine/IO/material_io.hpp"
#include "Engine/Scene/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace lve {
  namespace fs = std::filesystem;

  namespace {
    bool transformsEqual(const TransformComponent &a, const TransformComponent &b) {
      return a.translation == b.translation &&
        a.rotation == b.rotation &&
        a.scale == b.scale;
    }

    bool nodeOverridesEqual(
      const std::vector<NodeTransformOverride> &a,
      const std::vector<NodeTransformOverride> &b) {
      if (a.size() != b.size()) return false;
      for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].enabled != b[i].enabled ||
            !transformsEqual(a[i].transform, b[i].transform)) {
          return false;
        }
      }
      return true;
    }

    bool snapshotsEqual(
      const editor::GameObjectSnapshot &a,
      const editor::GameObjectSnapshot &b) {
      return a.id == b.id &&
        a.isSprite == b.isSprite &&
        a.isPointLight == b.isPointLight &&
        a.isCamera == b.isCamera &&
        transformsEqual(a.transform, b.transform) &&
        a.color == b.color &&
        a.lightIntensity == b.lightIntensity &&
        a.objState == b.objState &&
        a.billboardMode == b.billboardMode &&
        a.spriteMetaPath == b.spriteMetaPath &&
        a.spriteStateName == b.spriteStateName &&
        a.modelPath == b.modelPath &&
        a.materialPath == b.materialPath &&
        a.camera.projection == b.camera.projection &&
        a.camera.fov == b.camera.fov &&
        a.camera.orthoHeight == b.camera.orthoHeight &&
        a.camera.nearPlane == b.camera.nearPlane &&
        a.camera.farPlane == b.camera.farPlane &&
        a.camera.active == b.camera.active &&
        nodeOverridesEqual(a.nodeOverrides, b.nodeOverrides) &&
        a.name == b.name;
    }

    void applySceneSettingsToSnapshot(
      editor::GameObjectSnapshot &snapshot,
      const editor::SceneSettingsSnapshot &settings) {
      snapshot.color = settings.color;
      snapshot.lightIntensity = settings.lightIntensity;
      snapshot.objState = settings.objState;
      snapshot.billboardMode = settings.billboardMode;
      snapshot.spriteStateName = settings.spriteStateName;
      if (settings.camera) {
        snapshot.camera = *settings.camera;
      }
    }
  } // namespace

  bool EditorSystem::applyHistoryActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem) {
    auto &history = getHistory();
    bool historyTriggered = false;
    const int undoSteps = result.undoSteps + (result.undoRequested ? 1 : 0);
    const int redoSteps = result.redoSteps + (result.redoRequested ? 1 : 0);
    if (undoSteps > 0) {
      for (int i = 0; i < undoSteps; ++i) {
        if (!history.undo()) break;
        historyTriggered = true;
      }
    } else if (redoSteps > 0) {
      for (int i = 0; i < redoSteps; ++i) {
        if (!history.redo()) break;
        historyTriggered = true;
      }
    }
    if (historyTriggered) {
      markSceneDirty("History changed scene");
    }

    if (!historyTriggered && result.selectedObject) {
      const auto selectedId = result.selectedObject->getId();
      if (result.inspectorActions.transformChanged && result.inspectorActions.transformCommitted) {
        const auto before = result.inspectorActions.beforeTransform;
        const auto after = result.inspectorActions.afterTransform;
        history.push({
          "Transform",
          [&, selectedId, before]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->transform.translation = before.translation;
            obj->transform.rotation = before.rotation;
            obj->transform.scale = before.scale;
            obj->transformDirty = true;
          },
          [&, selectedId, after]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->transform.translation = after.translation;
            obj->transform.rotation = after.rotation;
            obj->transform.scale = after.scale;
            obj->transformDirty = true;
          }});
        markSceneDirty("Transform modified");
      }
      if (result.inspectorActions.nameChanged) {
        const std::string beforeName = result.inspectorActions.beforeName;
        const std::string afterName = result.inspectorActions.afterName;
        history.push({
          "Rename",
          [&, selectedId, beforeName]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->name = beforeName;
          },
          [&, selectedId, afterName]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->name = afterName;
          }});
        markSceneDirty("Object renamed");
      }
      if (result.inspectorActions.nodeOverridesChanged &&
          result.inspectorActions.nodeOverridesCommitted) {
        const auto before = result.inspectorActions.beforeNodeOverrides;
        const auto after = result.inspectorActions.afterNodeOverrides;
        history.push({
          "Node Override",
          [&, selectedId, before]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj || !obj->model) return;
            sceneSystem.ensureNodeOverrides(*obj);
            auto &target = obj->nodeOverrides;
            for (auto &override : target) {
              override.enabled = false;
              override.transform = TransformComponent{};
            }
            const std::size_t count = std::min(target.size(), before.size());
            for (std::size_t i = 0; i < count; ++i) {
              target[i] = before[i];
            }
          },
          [&, selectedId, after]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj || !obj->model) return;
            sceneSystem.ensureNodeOverrides(*obj);
            auto &target = obj->nodeOverrides;
            for (auto &override : target) {
              override.enabled = false;
              override.transform = TransformComponent{};
            }
            const std::size_t count = std::min(target.size(), after.size());
            for (std::size_t i = 0; i < count; ++i) {
              target[i] = after[i];
            }
          }});
        markSceneDirty("Node override modified");
      }
    }

    return historyTriggered;
  }

  void EditorSystem::applyResourceActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    editor::ResourceBrowserState &resourceBrowserState) {
    auto pushObjectEdit = [&](const char *label,
                              const editor::GameObjectSnapshot &before,
                              const editor::GameObjectSnapshot &after) {
      if (snapshotsEqual(before, after)) {
        return;
      }
      history.push({
        label,
        [&, before]() {
          editor::ApplySnapshot(sceneSystem, animator, before);
          setSelectedId(before.id);
        },
        [&, after]() {
          editor::ApplySnapshot(sceneSystem, animator, after);
          setSelectedId(after.id);
        }});
    };

    if (result.resourceActions.setActiveSpriteMeta) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
    }

    if (result.resourceActions.setActiveMesh &&
        !resourceBrowserState.activeMeshPath.empty()) {
      sceneSystem.setActiveMeshPath(resourceBrowserState.activeMeshPath);
    }

    if (result.resourceActions.setActiveMaterial &&
        !resourceBrowserState.activeMaterialPath.empty()) {
      sceneSystem.setActiveMaterialPath(resourceBrowserState.activeMaterialPath);
      sceneSystem.loadMaterialCached(resourceBrowserState.activeMaterialPath);
    }

    if (result.resourceActions.applySpriteMetaToSelection &&
        result.selectedObject &&
        result.selectedObject->isSprite) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        result.selectedObject->spriteMetaPath = resourceBrowserState.activeSpriteMetaPath;
        if (animator) {
          const std::string &stateName = result.selectedObject->spriteStateName;
          if (!stateName.empty()) {
            animator->applySpriteState(*result.selectedObject, stateName);
          } else {
            animator->applySpriteState(*result.selectedObject, result.selectedObject->objState);
          }
        }
        animator = sceneSystem.getSpriteAnimator();
        const auto after = editor::CaptureSnapshot(*result.selectedObject);
        pushObjectEdit("Sprite Metadata", before, after);
        markSceneDirty("Sprite metadata changed");
      }
    }

    if (result.resourceActions.applyMeshToSelection &&
        result.selectedObject &&
        result.selectedObject->model) {
      const std::string meshPath = resourceBrowserState.activeMeshPath.empty()
        ? "Assets/models/colored_cube.obj"
        : resourceBrowserState.activeMeshPath;
      try {
        const auto before = editor::CaptureSnapshot(*result.selectedObject);
        result.selectedObject->model = sceneSystem.loadModelCached(meshPath);
        result.selectedObject->modelPath = meshPath;
        result.selectedObject->enableTextureType =
          result.selectedObject->model && result.selectedObject->model->hasAnyDiffuseTexture() ? 1 : 0;
        result.selectedObject->nodeOverrides.clear();
        sceneSystem.ensureNodeOverrides(*result.selectedObject);
        hierarchyState.selectedNodeIndex = -1;
        if (!result.selectedObject->materialPath.empty()) {
          sceneSystem.applyMaterialToObject(*result.selectedObject, result.selectedObject->materialPath);
        }
        const auto after = editor::CaptureSnapshot(*result.selectedObject);
        pushObjectEdit("Mesh", before, after);
        markSceneDirty("Mesh changed");
      } catch (const std::exception &e) {
        std::cerr << "Failed to load mesh " << meshPath << ": " << e.what() << "\n";
      }
    }

    if (result.resourceActions.applyMaterialToSelection &&
        result.selectedObject &&
        result.selectedObject->model) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      if (!sceneSystem.applyMaterialToObject(*result.selectedObject, resourceBrowserState.activeMaterialPath)) {
        std::cerr << "Failed to apply material " << resourceBrowserState.activeMaterialPath << "\n";
      } else {
        const auto after = editor::CaptureSnapshot(*result.selectedObject);
        pushObjectEdit("Material", before, after);
        markSceneDirty("Material changed");
      }
    }
  }

  void EditorSystem::applyInspectorActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    editor::ResourceBrowserState &resourceBrowserState) {
    auto pushObjectEdit = [&](const char *label,
                              const editor::GameObjectSnapshot &before,
                              const editor::GameObjectSnapshot &after,
                              std::optional<LveGameObject::id_t> activeCameraBefore = std::nullopt) {
      if (snapshotsEqual(before, after)) {
        return false;
      }
      history.push({
        label,
        [&, before, activeCameraBefore]() {
          editor::ApplySnapshot(sceneSystem, animator, before);
          if (activeCameraBefore) {
            sceneSystem.setActiveCamera(*activeCameraBefore, true);
          }
          setSelectedId(before.id);
        },
        [&, after]() {
          editor::ApplySnapshot(sceneSystem, animator, after);
          setSelectedId(after.id);
        }});
      return true;
    };

    if (result.inspectorActions.cameraActiveChanged &&
        result.selectedObject &&
        result.selectedObject->camera) {
      sceneSystem.setActiveCamera(
        result.selectedObject->getId(),
        result.inspectorActions.cameraActive);
    }

    if ((result.inspectorActions.cameraActiveChanged ||
         result.inspectorActions.sceneSettingsChanged) &&
        result.selectedObject &&
        result.selectedSnapshotBeforeUi) {
      auto before = *result.selectedSnapshotBeforeUi;
      applySceneSettingsToSnapshot(before, result.inspectorActions.beforeSceneSettings);
      const auto after = editor::CaptureSnapshot(*result.selectedObject);
      if (pushObjectEdit(
            "Object Settings",
            before,
            after,
            result.activeCameraBeforeUi)) {
        markSceneDirty(
          result.inspectorActions.cameraActiveChanged
            ? "Active camera changed"
            : "Scene settings changed");
      }
    }

    if (result.inspectorActions.materialPreviewRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      const std::string &path = result.inspectorActions.materialPath;
      if (!path.empty()) {
        sceneSystem.updateMaterialFromData(path, result.inspectorActions.materialData);
        if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
          std::cerr << "Failed to apply material " << path << "\n";
        } else {
          resourceBrowserState.activeMaterialPath = path;
          const auto after = editor::CaptureSnapshot(*result.selectedObject);
          if (pushObjectEdit("Material", before, after)) {
            markSceneDirty("Material changed");
          }
        }
      }
    }

    if (result.inspectorActions.materialPickRequested) {
      fileDialogPurpose = FileDialogPurpose::MaterialTexture;
      pendingMaterialPickSlot = result.inspectorActions.materialPickSlot;
      showFileDialog = true;
      fileDialogState.title = "Select Texture";
      fileDialogState.okLabel = "Select";
      fileDialogState.allowDirectories = false;
      fileDialogState.browser.restrictToRoot = true;
      fileDialogState.browser.filter.clear();
      const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
        ? "Assets"
        : resourceBrowserState.browser.rootPath;
      fileDialogState.browser.rootPath = rootPath;
      fileDialogState.browser.currentPath = rootPath;
      fileDialogState.browser.pendingRefresh = true;
    }

    if (result.inspectorActions.materialClearRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      sceneSystem.applyMaterialToObject(*result.selectedObject, {});
      const auto after = editor::CaptureSnapshot(*result.selectedObject);
      if (pushObjectEdit("Material Clear", before, after)) {
        markSceneDirty("Material cleared");
      }
    }

    if (result.inspectorActions.materialLoadRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      const std::string &path = result.inspectorActions.materialPath;
      if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
        std::cerr << "Failed to apply material " << path << "\n";
      } else {
        resourceBrowserState.activeMaterialPath = path;
        const auto after = editor::CaptureSnapshot(*result.selectedObject);
        if (pushObjectEdit("Material", before, after)) {
          markSceneDirty("Material changed");
        }
      }
    }

    if (result.inspectorActions.materialSaveRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const auto before = editor::CaptureSnapshot(*result.selectedObject);
      const std::string &path = result.inspectorActions.materialPath;
      std::string error;
      if (!saveMaterialToFile(path, result.inspectorActions.materialData, &error)) {
        std::cerr << "Failed to save material " << path;
        if (!error.empty()) {
          std::cerr << ": " << error;
        }
        std::cerr << "\n";
      } else {
        sceneSystem.getAssetDatabase().registerAsset(path);
        sceneSystem.updateMaterialFromData(path, result.inspectorActions.materialData);
        if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
          std::cerr << "Failed to apply material " << path << "\n";
        } else {
          resourceBrowserState.activeMaterialPath = path;
          const auto after = editor::CaptureSnapshot(*result.selectedObject);
          if (pushObjectEdit("Material", before, after)) {
            markSceneDirty("Material changed");
          }
        }
      }
    }
    (void)animator;
  }

  void EditorSystem::handlePicking(
    EditorFrameResult &result,
    const std::vector<LveGameObject*> &objects,
    const glm::mat4 &view,
    const glm::mat4 &projection) {
    if (result.sceneView.leftMouseClicked && result.sceneView.allowPick) {
      const editor::tools::Ray ray = editor::tools::BuildPickRay(result.sceneView, view, projection);
      if (ray.valid) {
        float bestT = std::numeric_limits<float>::max();
        std::optional<LveGameObject::id_t> hitId{};
        std::optional<int> hitNodeIndex{};
        const glm::mat4 invView = glm::inverse(view);
        const glm::vec3 camRight = glm::vec3(invView[0]);
        const glm::vec3 camUp = glm::vec3(invView[1]);

        for (auto *obj : objects) {
          if (!obj) continue;
          if (!obj->model && !obj->pointLight && !obj->isSprite) continue;
          float tHitWorld = std::numeric_limits<float>::max();
          bool hit = false;
          if (obj->pointLight) {
            float tHit = 0.f;
            if (editor::tools::IntersectSphere(ray, obj->transform.translation, obj->transform.scale.x, tHit)) {
              tHitWorld = tHit;
              hit = true;
            }
          } else if (obj->isSprite) {
            float tHit = 0.f;
            glm::vec2 halfSize{
              std::abs(obj->transform.scale.x) * 0.5f,
              std::abs(obj->transform.scale.y) * 0.5f};
            if (editor::tools::IntersectBillboardQuad(
                  ray,
                  obj->transform.translation,
                  camRight,
                  camUp,
                  halfSize,
                  tHit)) {
              tHitWorld = tHit - 0.01f; // slight bias toward sprites
              hit = true;
            }
          } else if (obj->model) {
            const auto &nodes = obj->model->getNodes();
            const auto &subMeshes = obj->model->getSubMeshes();
            if (!nodes.empty() && !subMeshes.empty()) {
              std::vector<glm::mat4> localOverrides(nodes.size(), glm::mat4(1.f));
              if (obj->nodeOverrides.size() == nodes.size()) {
                for (std::size_t i = 0; i < nodes.size(); ++i) {
                  const auto &override = obj->nodeOverrides[i];
                  if (override.enabled) {
                    localOverrides[i] = override.transform.mat4();
                  }
                }
              }
              std::vector<glm::mat4> nodeGlobals;
              obj->model->computeNodeGlobals(localOverrides, nodeGlobals);

              const glm::mat4 objectTransform = obj->transform.mat4();
              for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const auto &node = nodes[nodeIndex];
                if (node.meshes.empty()) continue;

                bool hasNodeBounds = false;
                glm::vec3 nodeMin;
                glm::vec3 nodeMax;
                const glm::mat4 nodeTransform = objectTransform * nodeGlobals[nodeIndex];
                for (int meshIndex : node.meshes) {
                  if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= subMeshes.size()) {
                    continue;
                  }
                  const auto &subMesh = subMeshes[static_cast<std::size_t>(meshIndex)];
                  if (!subMesh.hasBounds) continue;
                  glm::vec3 worldMin;
                  glm::vec3 worldMax;
                  editor::tools::TransformAabb(nodeTransform, subMesh.boundsMin, subMesh.boundsMax, worldMin, worldMax);
                  if (!hasNodeBounds) {
                    nodeMin = worldMin;
                    nodeMax = worldMax;
                    hasNodeBounds = true;
                  } else {
                    nodeMin = glm::min(nodeMin, worldMin);
                    nodeMax = glm::max(nodeMax, worldMax);
                  }
                }

                if (!hasNodeBounds) continue;
                float tHit = 0.f;
                if (editor::tools::IntersectAabbLocal(ray.origin, ray.direction, nodeMin, nodeMax, tHit)) {
                  tHitWorld = tHit;
                  hit = true;
                  if (tHitWorld < bestT) {
                    bestT = tHitWorld;
                    hitId = obj->getId();
                    hitNodeIndex = static_cast<int>(nodeIndex);
                  }
                }
              }
            } else {
              const auto &bbox = obj->model->getBoundingBox();
              glm::mat4 modelMat = obj->transform.mat4();
              glm::mat4 invModel = glm::inverse(modelMat);
              glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(ray.origin, 1.f));
              glm::vec3 localDir = glm::normalize(glm::mat3(invModel) * ray.direction);
              float tLocal = 0.f;
              if (editor::tools::IntersectAabbLocal(localOrigin, localDir, bbox.min, bbox.max, tLocal)) {
                glm::vec3 hitLocal = localOrigin + localDir * tLocal;
                glm::vec3 hitWorld = glm::vec3(modelMat * glm::vec4(hitLocal, 1.f));
                tHitWorld = glm::length(hitWorld - ray.origin);
                hit = true;
              }
            }
          }

          if (hit && tHitWorld < bestT) {
            bestT = tHitWorld;
            hitId = obj->getId();
          }
        }
        if (hitId) {
          setSelectedId(hitId);
          if (hitNodeIndex.has_value()) {
            setSelectedNodeIndex(*hitNodeIndex);
          }
        }
      }
    }
  }

  void EditorSystem::handleCreateDelete(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::vec3 &cameraPos,
    editor::ResourceBrowserState &resourceBrowserState,
    LveGameObject::id_t protectedId,
    bool historyTriggered) {
    glm::vec3 spawnForward{0.f, 0.f, 1.f};
    glm::vec3 spawnOrigin = cameraPos;
    {
      glm::mat4 invView = glm::inverse(view);
      spawnOrigin = glm::vec3(invView[3]);
      glm::vec3 forward{invView[2][0], invView[2][1], invView[2][2]};
      if (glm::length(forward) > 0.0001f) {
        spawnForward = glm::normalize(forward);
      }
    }
    const glm::vec3 spawnPos = spawnOrigin + spawnForward * 2.f;
    const std::string meshPathForNew = resourceBrowserState.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : resourceBrowserState.activeMeshPath;
    const std::string spriteMetaForNew = resourceBrowserState.activeSpriteMetaPath.empty()
      ? "Assets/textures/characters/player.json"
      : resourceBrowserState.activeSpriteMetaPath;

    switch (result.hierarchyActions.createRequest) {
      case editor::HierarchyCreateRequest::Sprite: {
        auto &obj = sceneSystem.createSpriteObject(spawnPos, ObjectState::IDLE, spriteMetaForNew);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        markSceneDirty("Sprite created");
        if (!historyTriggered) {
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Sprite",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::Mesh: {
        auto &obj = sceneSystem.createMeshObject(spawnPos, meshPathForNew);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        markSceneDirty("Mesh created");
        {
          std::string instancePath;
          std::string error;
          const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
            ? "Assets"
            : resourceBrowserState.browser.rootPath;
          const std::string sourceMaterial = resourceBrowserState.activeMaterialPath;
          if (editor::workflow::CreateMaterialInstance(
                sceneSystem,
                sourceMaterial,
                obj.model.get(),
                obj.getId(),
                rootPath,
                instancePath,
                error)) {
            if (!sceneSystem.applyMaterialToObject(obj, instancePath)) {
              std::cerr << "Failed to apply material " << instancePath << "\n";
            }
          } else {
            if (!error.empty()) {
              std::cerr << "Failed to create material instance: " << error << "\n";
            }
            if (!sourceMaterial.empty()) {
              sceneSystem.applyMaterialToObject(obj, sourceMaterial);
            }
          }
        }
        if (!historyTriggered) {
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Mesh",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::PointLight: {
        auto &obj = sceneSystem.createPointLightObject(spawnPos);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        markSceneDirty("Light created");
        if (!historyTriggered) {
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Light",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::Camera: {
        std::optional<LveGameObject::id_t> activeCameraBeforeCreate{};
        if (auto *activeCamera = sceneSystem.findActiveCamera()) {
          activeCameraBeforeCreate = activeCamera->getId();
        }
        auto &obj = sceneSystem.createCameraObject(spawnPos);
        sceneSystem.setActiveCamera(obj.getId(), true);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        markSceneDirty("Camera created");
        if (!historyTriggered) {
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Camera",
            [&, id = obj.getId(), activeCameraBeforeCreate]() {
              sceneSystem.destroyObject(id);
              if (activeCameraBeforeCreate) {
                sceneSystem.setActiveCamera(*activeCameraBeforeCreate, true);
              }
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::None:
      default: break;
    }

    if (result.duplicateSelectedRequested) {
      auto selectedId = getSelectedId();
      if (selectedId && *selectedId != protectedId) {
        if (auto *obj = sceneSystem.findObject(*selectedId)) {
          auto cloneSource = editor::CaptureSnapshot(*obj);
          cloneSource.id = 0;
          cloneSource.name = cloneSource.name.empty()
            ? "Duplicate"
            : cloneSource.name + " Copy";
          cloneSource.transform.translation += glm::vec3{0.5f, 0.f, 0.5f};
          if (cloneSource.isCamera) {
            cloneSource.camera.active = false;
          }
          auto &duplicate = editor::CloneSnapshot(sceneSystem, animator, cloneSource);
          const auto duplicateSnapshot = editor::CaptureSnapshot(duplicate);
          setSelectedId(duplicate.getId());
          markSceneDirty("Object duplicated");
          if (!historyTriggered) {
            history.push({
              "Duplicate Object",
              [&, id = duplicate.getId()]() {
                sceneSystem.destroyObject(id);
                setSelectedId(std::nullopt);
              },
              [&, duplicateSnapshot]() {
                editor::RestoreSnapshot(sceneSystem, animator, duplicateSnapshot);
                setSelectedId(duplicateSnapshot.id);
              }});
          }
        }
      }
    }

    if (result.hierarchyActions.deleteSelected) {
      auto selectedId = getSelectedId();
      if (selectedId && *selectedId != protectedId) {
        editor::GameObjectSnapshot snapshot{};
        bool hasSnapshot = false;
        auto *obj = sceneSystem.findObject(*selectedId);
        if (obj) {
          snapshot = editor::CaptureSnapshot(*obj);
          hasSnapshot = true;
        }
        if (sceneSystem.destroyObject(*selectedId)) {
          setSelectedId(std::nullopt);
          markSceneDirty("Object deleted");
          if (!historyTriggered && hasSnapshot) {
            history.push({
              "Delete Object",
              [&, snapshot]() {
                editor::RestoreSnapshot(sceneSystem, animator, snapshot);
                setSelectedId(snapshot.id);
              },
              [&, id = *selectedId]() {
                sceneSystem.destroyObject(id);
                setSelectedId(std::nullopt);
              }});
          }
        }
      }
    }
  }

  void EditorSystem::handleSceneActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    LveGameObject::id_t viewerId) {
    if (result.sceneActions.saveRequested) {
      const std::string path = getScenePanelState().path;
      if (sceneSystem.saveSceneToFile(path)) {
        markSceneClean(path, "Scene saved");
      } else {
        scenePanelState.statusMessage = "Save failed";
      }
    }
    if (result.sceneActions.loadRequested) {
      const std::string path = getScenePanelState().path;
      renderBackend.waitIdle();
      if (sceneSystem.loadSceneFromFile(path, viewerId)) {
        animator = sceneSystem.getSpriteAnimator();
        history.clear();
        setSelectedId(std::nullopt);
        markSceneClean(path, "Scene loaded");
      } else {
        scenePanelState.statusMessage = "Load failed";
      }
    }
  }


} // namespace lve
