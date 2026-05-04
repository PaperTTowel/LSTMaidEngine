#include "sprite_metadata.hpp"

#include "Engine/IO/json.hpp"
#include "Engine/Core/runtime_paths.hpp"

// std
#include <fstream>
#include <sstream>
#include <string>

namespace lve {
  namespace {
    std::string readFileToString(const std::string &path) {
      std::ifstream file(RuntimePaths::resolveResourcePath(path), std::ios::in | std::ios::binary);
      if (!file) {
        return {};
      }
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    int readInt(const io::JsonValue &src, const std::string &key, int defaultValue) {
      const auto *value = src.find(key);
      return value ? value->asInt(defaultValue) : defaultValue;
    }

    float readFloat(const io::JsonValue &src, const std::string &key, float defaultValue) {
      const auto *value = src.find(key);
      return value ? static_cast<float>(value->asNumber(defaultValue)) : defaultValue;
    }

    bool readBool(const io::JsonValue &src, const std::string &key, bool defaultValue) {
      const auto *value = src.find(key);
      return value ? value->asBool(defaultValue) : defaultValue;
    }

    std::string readString(const io::JsonValue &src, const std::string &key, const std::string &defaultValue) {
      const auto *value = src.find(key);
      return value ? value->asString(defaultValue) : defaultValue;
    }

    glm::vec2 readVec2(const io::JsonValue &src, const std::string &key, glm::vec2 defaultValue) {
      const auto *value = src.find(key);
      const auto *array = value ? value->asArray() : nullptr;
      if (array && array->size() >= 2) {
        return glm::vec2{
          static_cast<float>((*array)[0].asNumber(defaultValue.x)),
          static_cast<float>((*array)[1].asNumber(defaultValue.y))};
      }
      return defaultValue;
    }
  } // namespace
  
  bool loadSpriteMetadata(const std::string &filepath, SpriteMetadata &outMetadata) {
    const std::string content = readFileToString(filepath);
    if (content.empty()) {
      return false;
    }

    io::JsonValue root;
    if (!io::parseJson(content, root) || !root.isObject()) {
      return false;
    }

    outMetadata.atlasCols = readInt(root, "cols", outMetadata.atlasCols);
    outMetadata.atlasRows = readInt(root, "rows", outMetadata.atlasRows);
    outMetadata.size = readVec2(root, "size", outMetadata.size);
    outMetadata.pivot = readVec2(root, "pivot", outMetadata.pivot);

    const auto *states = root.find("states");
    const auto *stateObject = states ? states->asObject() : nullptr;
    if (stateObject) {
      for (const auto &kv : *stateObject) {
        const std::string &stateName = kv.first;
        const auto &body = kv.second;
        if (!body.isObject()) continue;

        SpriteStateInfo state{};
        state.texturePath = readString(body, "texture", "");
        state.row = readInt(body, "row", state.row);
        state.frameCount = readInt(body, "frames", state.frameCount);
        float fps = readFloat(body, "fps", 0.0f);
        if (fps > 0.0f) {
          state.frameDuration = 1.0f / fps;
        } else {
          state.frameDuration = readFloat(body, "frameDuration", state.frameDuration);
        }
        state.loop = readBool(body, "loop", state.loop);
        state.atlasCols = readInt(body, "cols", state.atlasCols);
        state.atlasRows = readInt(body, "rows", state.atlasRows);
        outMetadata.states[stateName] = state;
      }
    }

    return true;
  }

} // namespace lve
