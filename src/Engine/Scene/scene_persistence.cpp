#include "Engine/Scene/scene_persistence.hpp"

#include "Engine/Scene/scene_system.hpp"

#include <iostream>
#include <memory>
#include <utility>

namespace lve {
  namespace {
    bool isIdentityTransform(const TransformComponent &transform) {
      const float eps = 0.0001f;
      return glm::length(transform.translation) < eps &&
        glm::length(transform.rotation) < eps &&
        glm::length(transform.scale - glm::vec3(1.f)) < eps;
    }

    std::optional<LveGameObject::id_t> parseObjectId(const std::string &id) {
      constexpr const char *prefix = "obj_";
      constexpr std::size_t prefixLength = 4;
      if (id.size() <= prefixLength || id.compare(0, prefixLength, prefix) != 0) {
        return std::nullopt;
      }

      LveGameObject::id_t parsed = 0;
      for (std::size_t i = prefixLength; i < id.size(); ++i) {
        const char ch = id[i];
        if (ch < '0' || ch > '9') {
          return std::nullopt;
        }
        parsed = parsed * 10u + static_cast<LveGameObject::id_t>(ch - '0');
      }
      return parsed;
    }
  } // namespace

  Scene ScenePersistence::exportSnapshot(SceneSystem &sceneSystem) {
    Scene scene{};
    scene.version = 1;
    scene.resources.basePath = "Assets/";
    scene.resources.spritePath = "Assets/textures/characters/";
    scene.resources.modelPath = "Assets/models/";
    scene.resources.materialPath = "Assets/materials/";

    for (const auto &kv : sceneSystem.gameObjectManager.gameObjects) {
      const auto &obj = kv.second;
      if (!obj.model && !obj.pointLight && !obj.isSprite && !obj.camera) {
        continue;
      }
      SceneEntity e{};
      e.id = "obj_" + std::to_string(obj.getId());
      e.name = obj.name.empty() ? e.id : obj.name;
      e.transform.position = obj.transform.translation;
      e.transform.rotation = obj.transform.rotation;
      e.transform.scale = obj.transform.scale;

      if (obj.pointLight) {
        e.type = EntityType::Light;
        LightComponent lc{};
        lc.kind = LightKind::Point;
        lc.color = obj.color;
        lc.intensity = obj.pointLight->lightIntensity;
        lc.range = 10.f;
        lc.angle = 45.f;
        e.light = lc;
      } else if (obj.isSprite) {
        e.type = EntityType::Sprite;
        SpriteComponent sc{};
        sc.spriteMeta = obj.spriteMetaPath.empty() ? "Assets/textures/characters/player.json" : obj.spriteMetaPath;
        sc.spriteMetaGuid = sceneSystem.assetService.assetDatabase().ensureMetaForAsset(sc.spriteMeta);
        sc.state = obj.spriteStateName.empty() ? sceneSystem.objectStateToString(obj.objState) : obj.spriteStateName;
        sc.billboard = (obj.billboardMode == BillboardMode::Spherical) ? BillboardKind::Spherical
          : (obj.billboardMode == BillboardMode::Cylindrical ? BillboardKind::Cylindrical : BillboardKind::None);
        sc.layer = 0;
        e.sprite = sc;
      } else if (obj.camera) {
        e.type = EntityType::Camera;
        e.camera = *obj.camera;
      } else {
        e.type = EntityType::Mesh;
        MeshComponent mc{};
        mc.model = obj.modelPath.empty() ? "Assets/models/colored_cube.obj" : obj.modelPath;
        mc.modelGuid = sceneSystem.assetService.assetDatabase().ensureMetaForAsset(mc.model);
        mc.material = obj.materialPath;
        if (!mc.material.empty()) {
          mc.materialGuid = sceneSystem.assetService.assetDatabase().ensureMetaForAsset(mc.material);
        }
        if (!obj.nodeOverrides.empty()) {
          for (std::size_t i = 0; i < obj.nodeOverrides.size(); ++i) {
            const auto &override = obj.nodeOverrides[i];
            if (!override.enabled || isIdentityTransform(override.transform)) {
              continue;
            }
            MeshComponent::NodeOverride nodeOverride{};
            nodeOverride.node = static_cast<int>(i);
            nodeOverride.transform.position = override.transform.translation;
            nodeOverride.transform.rotation = override.transform.rotation;
            nodeOverride.transform.scale = override.transform.scale;
            mc.nodeOverrides.push_back(std::move(nodeOverride));
          }
        }
        e.mesh = mc;
      }

      scene.entities.push_back(std::move(e));
    }

    return scene;
  }

  void ScenePersistence::importSnapshot(
    SceneSystem &sceneSystem,
    const Scene &scene,
    std::optional<LveGameObject::id_t> protectedId) {
    sceneSystem.gameObjectManager.clearAllExcept(protectedId);
    sceneSystem.cubeModel.reset();
    sceneSystem.spriteModel.reset();
    sceneSystem.assetService.clearCaches();

    auto resolveAssetPath = [&sceneSystem](const std::string &guid, const std::string &path) {
      if (!guid.empty()) {
        const std::string assetPath = sceneSystem.assetService.assetDatabase().getPathForGuid(guid);
        if (!assetPath.empty()) {
          return assetPath;
        }
      }
      return path;
    };

    std::string metaPath = sceneSystem.assetDefaults.activeSpriteMetaPath.empty()
      ? "Assets/textures/characters/player.json"
      : sceneSystem.assetDefaults.activeSpriteMetaPath;
    for (const auto &e : scene.entities) {
      if (e.sprite) {
        const std::string assetPath = resolveAssetPath(e.sprite->spriteMetaGuid, e.sprite->spriteMeta);
        if (!assetPath.empty()) {
          metaPath = assetPath;
        }
        break;
      }
    }
    if (!sceneSystem.setActiveSpriteMetadata(metaPath)) {
      std::cerr << "Falling back to previous sprite metadata\n";
      if (!sceneSystem.spriteAnimator) {
        sceneSystem.spriteAnimator = std::make_unique<SpriteAnimator>(
          sceneSystem.assetService.assetFactory(),
          sceneSystem.playerMeta,
          [&sceneSystem](const std::string &assetPath) {
            return sceneSystem.textureLoadOptionsForAsset(assetPath);
          });
      }
    }

    sceneSystem.characterId = 0;
    bool characterAssigned = false;
    std::optional<LveGameObject::id_t> activeCameraId{};
    for (const auto &e : scene.entities) {
      const std::optional<LveGameObject::id_t> parsedId = parseObjectId(e.id);
      const bool canRestoreId = parsedId && (!protectedId || *parsedId != *protectedId);
      if (e.type == EntityType::Light) {
        auto &light = canRestoreId
          ? sceneSystem.createPointLightObjectWithId(
              *parsedId,
              e.transform.position,
              e.light ? e.light->intensity : 0.2f,
              e.transform.scale.x,
              e.light ? e.light->color : glm::vec3(1.f))
          : sceneSystem.createPointLightObject(e.transform.position);
        light.color = e.light ? e.light->color : glm::vec3(1.f);
        if (light.pointLight && e.light) {
          light.pointLight->lightIntensity = e.light->intensity;
        }
        light.transform.rotation = e.transform.rotation;
        light.transform.scale = e.transform.scale;
        light.name = !e.name.empty() ? e.name : "PointLight " + std::to_string(light.getId());
        light.transformDirty = true;
        continue;
      }

      if (e.type == EntityType::Camera && e.camera) {
        auto &cameraObj = canRestoreId
          ? sceneSystem.createCameraObjectWithId(*parsedId, e.transform.position, *e.camera)
          : sceneSystem.createCameraObject(e.transform.position);
        cameraObj.transform.rotation = e.transform.rotation;
        cameraObj.transform.scale = e.transform.scale;
        cameraObj.name = !e.name.empty() ? e.name : "Camera " + std::to_string(cameraObj.getId());
        cameraObj.camera = *e.camera;
        cameraObj.transformDirty = true;
        if (e.camera->active && !activeCameraId) {
          activeCameraId = cameraObj.getId();
        }
        continue;
      }

      if (e.type == EntityType::Sprite && e.sprite) {
        ObjectState desiredState = sceneSystem.objectStateFromString(e.sprite->state);
        std::string spritePath = resolveAssetPath(e.sprite->spriteMetaGuid, e.sprite->spriteMeta);
        if (spritePath.empty()) {
          spritePath = metaPath;
        }
        auto &obj = canRestoreId
          ? sceneSystem.createSpriteObjectWithId(*parsedId, e.transform.position, desiredState, spritePath)
          : sceneSystem.createSpriteObject(e.transform.position, desiredState, spritePath);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Sprite " + std::to_string(obj.getId());
        obj.billboardMode = (e.sprite->billboard == BillboardKind::Spherical) ? BillboardMode::Spherical
          : (e.sprite->billboard == BillboardKind::Cylindrical ? BillboardMode::Cylindrical : BillboardMode::None);
        if (!e.sprite->state.empty()) {
          obj.spriteStateName = e.sprite->state;
          if (sceneSystem.spriteAnimator) {
            sceneSystem.spriteAnimator->applySpriteState(obj, obj.spriteStateName);
          }
        }
        obj.transformDirty = true;
        if (!characterAssigned) {
          sceneSystem.characterId = obj.getId();
          characterAssigned = true;
        }
        continue;
      }

      if (e.type == EntityType::Mesh && e.mesh) {
        const std::string modelPath = resolveAssetPath(e.mesh->modelGuid, e.mesh->model);
        auto &obj = canRestoreId
          ? sceneSystem.createMeshObjectWithId(*parsedId, e.transform.position, modelPath)
          : sceneSystem.createMeshObject(e.transform.position, modelPath);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Mesh " + std::to_string(obj.getId());
        obj.transformDirty = true;
        sceneSystem.applyNodeOverrides(obj, *e.mesh);
        const std::string materialPath = resolveAssetPath(e.mesh->materialGuid, e.mesh->material);
        if (!materialPath.empty()) {
          sceneSystem.applyMaterialToObject(obj, materialPath);
        }
        continue;
      }
    }

    if (activeCameraId) {
      sceneSystem.setActiveCamera(*activeCameraId, true);
    }

    if (!characterAssigned) {
      auto &characterObj = sceneSystem.createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, metaPath);
      sceneSystem.characterId = characterObj.getId();
    }
  }

  bool ScenePersistence::saveToFile(SceneSystem &sceneSystem, const std::string &path) {
    Scene scene = exportSnapshot(sceneSystem);
    if (!SceneSerializer::saveToFile(scene, path)) {
      std::cerr << "Failed to save scene to " << path << "\n";
      return false;
    }
    return true;
  }

  bool ScenePersistence::loadFromFile(
    SceneSystem &sceneSystem,
    const std::string &path,
    std::optional<LveGameObject::id_t> protectedId) {
    Scene scene{};
    if (!SceneSerializer::loadFromFile(path, scene)) {
      std::cerr << "Failed to load scene from " << path << "\n";
      return false;
    }
    importSnapshot(sceneSystem, scene, protectedId);
    return true;
  }

} // namespace lve
