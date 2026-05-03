#include "Engine/asset_database.hpp"

#include "Engine/IO/json.hpp"
#include "Engine/runtime_paths.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace lve {

  namespace {

    namespace fs = std::filesystem;

    std::string toLowerCopy(const std::string &value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool hasExtension(const fs::path &path, std::initializer_list<const char *> exts) {
      const std::string ext = toLowerCopy(path.extension().string());
      for (const char *candidate : exts) {
        if (ext == toLowerCopy(candidate)) {
          return true;
        }
      }
      return false;
    }

    AssetType typeFromString(const std::string &value) {
      const std::string key = toLowerCopy(value);
      if (key == "model") return AssetType::Model;
      if (key == "texture") return AssetType::Texture;
      if (key == "material") return AssetType::Material;
      if (key == "sprite") return AssetType::SpriteMeta;
      if (key == "scene") return AssetType::Scene;
      return AssetType::Unknown;
    }

    std::string typeToString(AssetType type) {
      switch (type) {
        case AssetType::Model: return "model";
        case AssetType::Texture: return "texture";
        case AssetType::Material: return "material";
        case AssetType::SpriteMeta: return "sprite";
        case AssetType::Scene: return "scene";
        default: return "unknown";
      }
    }

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

    std::string generateGuid() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dist(0, 255);
      unsigned char bytes[16];
      for (int i = 0; i < 16; ++i) {
        bytes[i] = static_cast<unsigned char>(dist(gen));
      }
      bytes[6] = (bytes[6] & 0x0F) | 0x40;
      bytes[8] = (bytes[8] & 0x3F) | 0x80;
      std::ostringstream ss;
      ss << std::hex;
      for (int i = 0; i < 16; ++i) {
        ss.width(2);
        ss.fill('0');
        ss << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
          ss << '-';
        }
      }
      return ss.str();
    }

    std::string normalizePathString(const fs::path &path) {
      return path.generic_string();
    }

    std::string makeAssetPath(const fs::path &root, const fs::path &absolutePath, const std::string &rootLabel) {
      std::error_code ec;
      fs::path relative = fs::relative(absolutePath, root, ec);
      if (ec) return normalizePathString(absolutePath);
      fs::path combined = fs::path(rootLabel) / relative;
      return normalizePathString(combined);
    }

    std::string metaPathForAsset(const std::string &assetPath) {
      return assetPath + ".meta";
    }

    AssetType detectJsonType(const fs::path &path) {
      const std::string content = readFileToString(path.string());
      io::JsonValue root;
      if (!io::parseJson(content, root) || !root.isObject()) {
        return AssetType::Unknown;
      }
      if (root.find("entities")) {
        return AssetType::Scene;
      }
      if (root.find("states")) {
        return AssetType::SpriteMeta;
      }
      return AssetType::Unknown;
    }

    AssetType detectAssetType(const std::string &assetPath) {
      fs::path path = assetPath;
      if (hasExtension(path, {".obj", ".fbx", ".gltf", ".glb"})) return AssetType::Model;
      if (hasExtension(path, {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".tiff", ".ktx", ".ktx2"})) {
        return AssetType::Texture;
      }
      if (hasExtension(path, {".mat"})) return AssetType::Material;
      if (hasExtension(path, {".json"})) {
        return detectJsonType(path);
      }
      return AssetType::Unknown;
    }

    bool saveMetaFile(const std::string &metaPath, const AssetMeta &meta) {
      std::ostringstream ss;
      ss << "{\n";
      ss << "  \"version\": " << meta.version << ",\n";
      ss << "  \"guid\": \"" << io::escapeJsonString(meta.guid) << "\",\n";
      ss << "  \"type\": \"" << io::escapeJsonString(typeToString(meta.type)) << "\",\n";
      ss << "  \"source\": \"" << io::escapeJsonString(meta.sourcePath) << "\"";
      if (meta.type == AssetType::Model) {
        ss << ",\n  \"import\": {\n";
        ss << "    \"scale\": " << meta.modelSettings.scale << ",\n";
        ss << "    \"generateNormals\": " << (meta.modelSettings.generateNormals ? "true" : "false") << ",\n";
        ss << "    \"generateTangents\": " << (meta.modelSettings.generateTangents ? "true" : "false") << ",\n";
        ss << "    \"flipUV\": " << (meta.modelSettings.flipUV ? "true" : "false") << "\n";
        ss << "  }";
      } else if (meta.type == AssetType::Texture) {
        ss << ",\n  \"import\": {\n";
        ss << "    \"sRGB\": " << (meta.textureSettings.sRGB ? "true" : "false") << ",\n";
        ss << "    \"generateMipmaps\": " << (meta.textureSettings.generateMipmaps ? "true" : "false") << "\n";
        ss << "  }";
      }
      ss << "\n}\n";
      return writeStringToFile(metaPath, ss.str());
    }

    bool loadMetaFile(const std::string &metaPath, AssetMeta &outMeta) {
      const std::string content = readFileToString(metaPath);
      if (content.empty()) return false;
      io::JsonValue root;
      if (!io::parseJson(content, root) || !root.isObject()) {
        return false;
      }
      outMeta.version = root.find("version") ? root.find("version")->asInt(outMeta.version) : outMeta.version;
      outMeta.guid = root.find("guid") ? root.find("guid")->asString(outMeta.guid) : outMeta.guid;
      outMeta.type = typeFromString(root.find("type") ? root.find("type")->asString(typeToString(outMeta.type)) : typeToString(outMeta.type));
      outMeta.sourcePath = root.find("source") ? root.find("source")->asString(outMeta.sourcePath) : outMeta.sourcePath;
      const auto *importSettings = root.find("import");
      if (outMeta.type == AssetType::Model) {
        const auto *scale = importSettings ? importSettings->find("scale") : nullptr;
        const auto *generateNormals = importSettings ? importSettings->find("generateNormals") : nullptr;
        const auto *generateTangents = importSettings ? importSettings->find("generateTangents") : nullptr;
        const auto *flipUV = importSettings ? importSettings->find("flipUV") : nullptr;
        outMeta.modelSettings.scale = scale ? static_cast<float>(scale->asNumber(outMeta.modelSettings.scale)) : outMeta.modelSettings.scale;
        outMeta.modelSettings.generateNormals = generateNormals ? generateNormals->asBool(outMeta.modelSettings.generateNormals) : outMeta.modelSettings.generateNormals;
        outMeta.modelSettings.generateTangents = generateTangents ? generateTangents->asBool(outMeta.modelSettings.generateTangents) : outMeta.modelSettings.generateTangents;
        outMeta.modelSettings.flipUV = flipUV ? flipUV->asBool(outMeta.modelSettings.flipUV) : outMeta.modelSettings.flipUV;
      } else if (outMeta.type == AssetType::Texture) {
        const auto *sRGB = importSettings ? importSettings->find("sRGB") : nullptr;
        const auto *generateMipmaps = importSettings ? importSettings->find("generateMipmaps") : nullptr;
        outMeta.textureSettings.sRGB = sRGB ? sRGB->asBool(outMeta.textureSettings.sRGB) : outMeta.textureSettings.sRGB;
        outMeta.textureSettings.generateMipmaps = generateMipmaps ? generateMipmaps->asBool(outMeta.textureSettings.generateMipmaps) : outMeta.textureSettings.generateMipmaps;
      }
      return true;
    }

  } // namespace

  AssetDatabase::AssetDatabase(std::string rootPath)
    : rootPath{std::move(rootPath)} {}

  void AssetDatabase::setRootPath(const std::string &newRootPath) {
    rootPath = newRootPath.empty() ? "Assets" : newRootPath;
  }

  void AssetDatabase::initialize() {
    pathToGuid.clear();
    guidToPath.clear();
    pathToMeta.clear();

    fs::path root = RuntimePaths::resolveResourcePath(rootPath);
    std::error_code ec;
    if (!fs::exists(root, ec)) {
      return;
    }

    for (fs::recursive_directory_iterator it(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec) break;
      if (!it->is_regular_file(ec)) continue;
      fs::path filePath = it->path();
      if (filePath.extension() == ".meta") continue;
      const std::string assetPath = makeAssetPath(root, filePath, rootPath);
      ensureMetaForAsset(assetPath);
    }
  }

  std::string AssetDatabase::registerAsset(const std::string &assetPath, const std::string &sourcePath) {
    return ensureMetaForAsset(assetPath, sourcePath);
  }

  std::string AssetDatabase::ensureMetaForAsset(const std::string &assetPath, const std::string &sourcePath) {
    if (assetPath.empty()) return {};
    fs::path assetFsPath = assetPath;
    std::string normalizedAssetPath = normalizePathString(assetFsPath);
    const std::string metaPath = metaPathForAsset(normalizedAssetPath);

    AssetMeta meta{};
    bool loaded = loadMetaFile(metaPath, meta);
    if (!loaded) {
      meta.guid = generateGuid();
      meta.type = detectAssetType(normalizedAssetPath);
    }
    if (meta.sourcePath.empty()) {
      meta.sourcePath = sourcePath.empty() ? normalizedAssetPath : sourcePath;
    } else if (!sourcePath.empty() && meta.sourcePath != sourcePath) {
      meta.sourcePath = sourcePath;
    }
    if (!saveMetaFile(metaPath, meta)) {
      return {};
    }

    pathToGuid[normalizedAssetPath] = meta.guid;
    guidToPath[meta.guid] = normalizedAssetPath;
    pathToMeta[normalizedAssetPath] = meta;
    return meta.guid;
  }

  std::string AssetDatabase::getGuidForPath(const std::string &assetPath) const {
    auto it = pathToGuid.find(assetPath);
    if (it == pathToGuid.end()) return {};
    return it->second;
  }

  std::string AssetDatabase::getPathForGuid(const std::string &guid) const {
    auto it = guidToPath.find(guid);
    if (it == guidToPath.end()) return {};
    return it->second;
  }

  std::string AssetDatabase::resolveAssetPath(const std::string &assetPath) const {
    auto it = pathToMeta.find(assetPath);
    if (it == pathToMeta.end()) return assetPath;
    if (!it->second.sourcePath.empty()) {
      return it->second.sourcePath;
    }
    return assetPath;
  }

  std::string AssetDatabase::resolveGuid(const std::string &guid) const {
    const std::string path = getPathForGuid(guid);
    if (path.empty()) return {};
    return resolveAssetPath(path);
  }

  const AssetMeta *AssetDatabase::getMetaForPath(const std::string &assetPath) const {
    auto it = pathToMeta.find(assetPath);
    if (it == pathToMeta.end()) return nullptr;
    return &it->second;
  }

  const AssetMeta *AssetDatabase::getMetaForGuid(const std::string &guid) const {
    auto it = guidToPath.find(guid);
    if (it == guidToPath.end()) return nullptr;
    return getMetaForPath(it->second);
  }

} // namespace lve
