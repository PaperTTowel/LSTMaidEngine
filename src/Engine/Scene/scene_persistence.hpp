#pragma once

#include "Engine/Scene/game_object.hpp"
#include "Engine/Scene/scene.hpp"

#include <optional>
#include <string>

namespace lve {
  class SceneSystem;

  class ScenePersistence {
  public:
    static Scene exportSnapshot(SceneSystem &sceneSystem);
    static void importSnapshot(
      SceneSystem &sceneSystem,
      const Scene &scene,
      std::optional<LveGameObject::id_t> protectedId);
    static void saveToFile(SceneSystem &sceneSystem, const std::string &path);
    static void loadFromFile(
      SceneSystem &sceneSystem,
      const std::string &path,
      std::optional<LveGameObject::id_t> protectedId);
  };
} // namespace lve
