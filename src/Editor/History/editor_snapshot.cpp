#include "Editor/History/editor_snapshot.hpp"

#include "Engine/Scene/scene_system.hpp"
#include "Engine/Assets/sprite_animator.hpp"

namespace lve::editor {

  GameObjectSnapshot CaptureSnapshot(const LveGameObject &obj) {
    GameObjectSnapshot snapshot{};
    snapshot.id = obj.getId();
    snapshot.isSprite = obj.isSprite;
    snapshot.isPointLight = obj.pointLight != nullptr;
    snapshot.isCamera = obj.camera.has_value();
    snapshot.transform = obj.transform;
    snapshot.color = obj.color;
    snapshot.objState = obj.objState;
    snapshot.billboardMode = obj.billboardMode;
    snapshot.spriteMetaPath = obj.spriteMetaPath;
    snapshot.spriteStateName = obj.spriteStateName;
    snapshot.modelPath = obj.modelPath;
    snapshot.materialPath = obj.materialPath;
    if (obj.camera) {
      snapshot.camera = *obj.camera;
    }
    snapshot.nodeOverrides = obj.nodeOverrides;
    snapshot.name = obj.name;
    if (obj.pointLight) {
      snapshot.lightIntensity = obj.pointLight->lightIntensity;
    }
    return snapshot;
  }

  void ApplySnapshot(
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    const GameObjectSnapshot &snapshot) {
    auto *obj = sceneSystem.findObject(snapshot.id);
    const bool typeMatches = obj &&
      snapshot.isSprite == obj->isSprite &&
      snapshot.isPointLight == (obj->pointLight != nullptr) &&
      snapshot.isCamera == obj->camera.has_value();

    if (!typeMatches) {
      if (obj) {
        sceneSystem.destroyObject(snapshot.id);
      }
      RestoreSnapshot(sceneSystem, animator, snapshot);
      return;
    }

    obj->transform = snapshot.transform;
    obj->color = snapshot.color;
    obj->name = snapshot.name;
    obj->objState = snapshot.objState;
    obj->billboardMode = snapshot.billboardMode;
    obj->spriteMetaPath = snapshot.spriteMetaPath;
    obj->spriteStateName = snapshot.spriteStateName;
    obj->modelPath = snapshot.modelPath;
    obj->nodeOverrides = snapshot.nodeOverrides;
    obj->transformDirty = true;

    if (snapshot.isPointLight && obj->pointLight) {
      obj->pointLight->lightIntensity = snapshot.lightIntensity;
      return;
    }

    if (snapshot.isCamera && obj->camera) {
      obj->camera = snapshot.camera;
      if (snapshot.camera.active) {
        sceneSystem.setActiveCamera(obj->getId(), true);
      }
      return;
    }

    if (snapshot.isSprite) {
      if (!snapshot.spriteMetaPath.empty() && sceneSystem.setActiveSpriteMetadata(snapshot.spriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
      if (animator) {
        if (!obj->spriteStateName.empty()) {
          animator->applySpriteState(*obj, obj->spriteStateName);
        } else {
          animator->applySpriteState(*obj, obj->objState);
        }
      }
      return;
    }

    if (!snapshot.modelPath.empty()) {
      obj->model = sceneSystem.loadModelCached(snapshot.modelPath);
      obj->enableTextureType =
        obj->model && obj->model->hasAnyDiffuseTexture() ? 1 : 0;
      sceneSystem.ensureNodeOverrides(*obj);
      obj->nodeOverrides = snapshot.nodeOverrides;
    }
    if (!sceneSystem.applyMaterialToObject(*obj, snapshot.materialPath)) {
      sceneSystem.applyMaterialToObject(*obj, {});
    }
  }

  void RestoreSnapshot(
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    const GameObjectSnapshot &snapshot) {
    if (snapshot.isPointLight) {
      auto &obj = sceneSystem.createPointLightObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.lightIntensity,
        snapshot.transform.scale.x,
        snapshot.color);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.name = snapshot.name;
      obj.transformDirty = true;
      return;
    }

    if (snapshot.isSprite) {
      auto &obj = sceneSystem.createSpriteObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.objState,
        snapshot.spriteMetaPath);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.billboardMode = snapshot.billboardMode;
      obj.name = snapshot.name;
      if (!snapshot.spriteStateName.empty()) {
        obj.spriteStateName = snapshot.spriteStateName;
      }
      if (!snapshot.spriteMetaPath.empty() && sceneSystem.setActiveSpriteMetadata(snapshot.spriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
      if (animator) {
        if (!obj.spriteStateName.empty()) {
          animator->applySpriteState(obj, obj.spriteStateName);
        } else {
          animator->applySpriteState(obj, obj.objState);
        }
      }
      obj.transformDirty = true;
      return;
    }

    if (snapshot.isCamera) {
      auto &obj = sceneSystem.createCameraObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.camera);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.name = snapshot.name;
      obj.transformDirty = true;
      if (snapshot.camera.active) {
        sceneSystem.setActiveCamera(obj.getId(), true);
      }
      return;
    }

    auto &obj = sceneSystem.createMeshObjectWithId(
      snapshot.id,
      snapshot.transform.translation,
      snapshot.modelPath);
    obj.transform.rotation = snapshot.transform.rotation;
    obj.transform.scale = snapshot.transform.scale;
    obj.name = snapshot.name;
    if (!snapshot.materialPath.empty()) {
      sceneSystem.applyMaterialToObject(obj, snapshot.materialPath);
    }
    if (!snapshot.nodeOverrides.empty()) {
      sceneSystem.ensureNodeOverrides(obj);
      obj.nodeOverrides = snapshot.nodeOverrides;
    }
    obj.transformDirty = true;
  }

  LveGameObject &CloneSnapshot(
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    const GameObjectSnapshot &snapshot) {
    if (snapshot.isPointLight) {
      auto &obj = sceneSystem.createPointLightObject(snapshot.transform.translation);
      const auto id = obj.getId();
      GameObjectSnapshot clone = snapshot;
      clone.id = id;
      ApplySnapshot(sceneSystem, animator, clone);
      return *sceneSystem.findObject(id);
    }

    if (snapshot.isSprite) {
      auto &obj = sceneSystem.createSpriteObject(
        snapshot.transform.translation,
        snapshot.objState,
        snapshot.spriteMetaPath);
      const auto id = obj.getId();
      GameObjectSnapshot clone = snapshot;
      clone.id = id;
      ApplySnapshot(sceneSystem, animator, clone);
      return *sceneSystem.findObject(id);
    }

    if (snapshot.isCamera) {
      auto &obj = sceneSystem.createCameraObject(snapshot.transform.translation);
      const auto id = obj.getId();
      GameObjectSnapshot clone = snapshot;
      clone.id = id;
      ApplySnapshot(sceneSystem, animator, clone);
      return *sceneSystem.findObject(id);
    }

    auto &obj = sceneSystem.createMeshObject(snapshot.transform.translation, snapshot.modelPath);
    const auto id = obj.getId();
    GameObjectSnapshot clone = snapshot;
    clone.id = id;
    ApplySnapshot(sceneSystem, animator, clone);
    return *sceneSystem.findObject(id);
  }

} // namespace lve::editor
