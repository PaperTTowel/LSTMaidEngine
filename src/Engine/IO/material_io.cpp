#include "Engine/IO/material_io.hpp"

#include "Engine/IO/json.hpp"
#include "Engine/runtime_paths.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace lve {

  namespace {
    std::string readFileToString(const std::string &path) {
      std::ifstream file(RuntimePaths::resolveResourcePath(path), std::ios::in | std::ios::binary);
      if (!file) return {};
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    bool writeStringToFile(const std::string &path, const std::string &data) {
      std::ofstream file(RuntimePaths::resolveResourcePath(path), std::ios::out | std::ios::binary | std::ios::trunc);
      if (!file) return false;
      file << data;
      return true;
    }

    std::string readString(const io::JsonValue &src, const std::string &key, const std::string &defVal) {
      const auto *value = src.find(key);
      return value ? value->asString(defVal) : defVal;
    }

    float readFloat(const io::JsonValue &src, const std::string &key, float defVal) {
      const auto *value = src.find(key);
      return value ? static_cast<float>(value->asNumber(defVal)) : defVal;
    }

    int readInt(const io::JsonValue &src, const std::string &key, int defVal) {
      const auto *value = src.find(key);
      return value ? value->asInt(defVal) : defVal;
    }

    glm::vec3 readVec3(const io::JsonValue &src, const std::string &key, const glm::vec3 &defVal) {
      const auto *value = src.find(key);
      const auto *array = value ? value->asArray() : nullptr;
      if (array && array->size() >= 3) {
        return glm::vec3{
          static_cast<float>((*array)[0].asNumber(defVal.x)),
          static_cast<float>((*array)[1].asNumber(defVal.y)),
          static_cast<float>((*array)[2].asNumber(defVal.z))};
      }
      return defVal;
    }

    glm::vec4 readVec4(const io::JsonValue &src, const std::string &key, const glm::vec4 &defVal) {
      const auto *value = src.find(key);
      const auto *array = value ? value->asArray() : nullptr;
      if (array && array->size() >= 4) {
        return glm::vec4{
          static_cast<float>((*array)[0].asNumber(defVal.x)),
          static_cast<float>((*array)[1].asNumber(defVal.y)),
          static_cast<float>((*array)[2].asNumber(defVal.z)),
          static_cast<float>((*array)[3].asNumber(defVal.w))};
      }
      return defVal;
    }

    std::string normalizeSlashes(const std::string &value) {
      std::string out = value;
      for (auto &ch : out) {
        if (ch == '\\') {
          ch = '/';
        }
      }
      return out;
    }

    MaterialData parseMaterialData(const io::JsonValue &root, const MaterialData &base) {
      MaterialData data = base;
      data.version = readInt(root, "version", data.version);
      data.name = readString(root, "name", data.name);
      data.textures.baseColor = readString(root, "baseColorTexture", data.textures.baseColor);
      data.textures.normal = readString(root, "normalTexture", data.textures.normal);
      data.textures.metallicRoughness = readString(root, "metallicRoughnessTexture", data.textures.metallicRoughness);
      data.textures.occlusion = readString(root, "occlusionTexture", data.textures.occlusion);
      data.textures.emissive = readString(root, "emissiveTexture", data.textures.emissive);
      data.factors.baseColor = readVec4(root, "baseColorFactor", data.factors.baseColor);
      data.factors.metallic = readFloat(root, "metallicFactor", data.factors.metallic);
      data.factors.roughness = readFloat(root, "roughnessFactor", data.factors.roughness);
      data.factors.emissive = readVec3(root, "emissiveFactor", data.factors.emissive);
      data.factors.occlusionStrength = readFloat(root, "occlusionStrength", data.factors.occlusionStrength);
      data.factors.normalScale = readFloat(root, "normalScale", data.factors.normalScale);
      return data;
    }

    std::string quoted(const std::string &value) {
      return "\"" + io::escapeJsonString(value) + "\"";
    }
  } // namespace

  bool saveMaterialToFile(
    const std::string &path,
    const MaterialData &data,
    std::string *outError) {
    if (path.empty()) {
      if (outError) {
        *outError = "Material path is empty";
      }
      return false;
    }

    std::filesystem::path outputPath(path);
    std::error_code ec;
    if (outputPath.has_parent_path()) {
      std::filesystem::create_directories(outputPath.parent_path(), ec);
    }
    if (ec) {
      if (outError) {
        *outError = "Failed to create material directory";
      }
      return false;
    }

    const std::string materialName = data.name.empty()
      ? outputPath.stem().string()
      : data.name;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": " << data.version << ",\n";
    ss << "  \"name\": " << quoted(materialName) << ",\n";
    ss << "  \"baseColorTexture\": " << quoted(normalizeSlashes(data.textures.baseColor)) << ",\n";
    ss << "  \"normalTexture\": " << quoted(normalizeSlashes(data.textures.normal)) << ",\n";
    ss << "  \"metallicRoughnessTexture\": " << quoted(normalizeSlashes(data.textures.metallicRoughness)) << ",\n";
    ss << "  \"occlusionTexture\": " << quoted(normalizeSlashes(data.textures.occlusion)) << ",\n";
    ss << "  \"emissiveTexture\": " << quoted(normalizeSlashes(data.textures.emissive)) << ",\n";
    ss << "  \"baseColorFactor\": [" << data.factors.baseColor.r << ", " << data.factors.baseColor.g << ", "
       << data.factors.baseColor.b << ", " << data.factors.baseColor.a << "],\n";
    ss << "  \"metallicFactor\": " << data.factors.metallic << ",\n";
    ss << "  \"roughnessFactor\": " << data.factors.roughness << ",\n";
    ss << "  \"emissiveFactor\": [" << data.factors.emissive.r << ", " << data.factors.emissive.g << ", "
       << data.factors.emissive.b << "],\n";
    ss << "  \"occlusionStrength\": " << data.factors.occlusionStrength << ",\n";
    ss << "  \"normalScale\": " << data.factors.normalScale << "\n";
    ss << "}\n";

    if (!writeStringToFile(path, ss.str())) {
      if (outError) {
        *outError = "Failed to write material file";
      }
      return false;
    }
    return true;
  }

  bool loadMaterialDataFromFile(
    const std::string &path,
    MaterialData &outData,
    std::string *outError,
    const std::function<std::string(const std::string &)> &pathResolver) {
    const std::string resolvedPath = pathResolver ? pathResolver(path) : path;
    const std::string content = readFileToString(resolvedPath);
    if (content.empty()) {
      if (outError) {
        *outError = "Failed to read material file";
      }
      return false;
    }

    io::JsonValue root;
    std::string parseError;
    if (!io::parseJson(content, root, &parseError) || !root.isObject()) {
      if (outError) {
        *outError = parseError.empty() ? "Invalid material JSON" : parseError;
      }
      return false;
    }

    outData = parseMaterialData(root, outData);
    return true;
  }

} // namespace lve
