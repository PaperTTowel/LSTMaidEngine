#include "scene.hpp"

#include "Engine/IO/json.hpp"
#include "Engine/Core/runtime_paths.hpp"

// std
#include <fstream>
#include <sstream>

namespace lve {

  namespace {
    std::string toString(EntityType type) {
      switch (type) {
        case EntityType::Sprite: return "sprite";
        case EntityType::Mesh: return "mesh";
        case EntityType::Light: return "light";
        case EntityType::Camera: return "camera";
        default: return "mesh";
      }
    }

    EntityType entityTypeFromString(const std::string &s) {
      if (s == "sprite") return EntityType::Sprite;
      if (s == "mesh") return EntityType::Mesh;
      if (s == "light") return EntityType::Light;
      if (s == "camera") return EntityType::Camera;
      return EntityType::Mesh;
    }

    std::string toString(BillboardKind kind) {
      switch (kind) {
        case BillboardKind::None: return "none";
        case BillboardKind::Cylindrical: return "cylindrical";
        case BillboardKind::Spherical: return "spherical";
        default: return "none";
      }
    }

    BillboardKind billboardFromString(const std::string &s) {
      if (s == "cylindrical") return BillboardKind::Cylindrical;
      if (s == "spherical") return BillboardKind::Spherical;
      if (s == "none") return BillboardKind::None;
      return BillboardKind::Cylindrical;
    }

    std::string toString(LightKind kind) {
      switch (kind) {
        case LightKind::Point: return "point";
        case LightKind::Spot: return "spot";
        case LightKind::Directional: return "dir";
        default: return "point";
      }
    }

    LightKind lightFromString(const std::string &s) {
      if (s == "point") return LightKind::Point;
      if (s == "spot") return LightKind::Spot;
      if (s == "dir" || s == "directional") return LightKind::Directional;
      return LightKind::Point;
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

    std::string indent(int level) {
      return std::string(level * 2, ' ');
    }

    void writeVec3(std::ostringstream &ss, const glm::vec3 &v) {
      ss << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    }

    std::string quoted(const std::string &value) {
      return "\"" + io::escapeJsonString(value) + "\"";
    }

    glm::vec3 readVec3(const io::JsonValue &src, const std::string &key, glm::vec3 defVal) {
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

    float readFloat(const io::JsonValue &src, const std::string &key, float defVal) {
      const auto *value = src.find(key);
      return value ? static_cast<float>(value->asNumber(defVal)) : defVal;
    }

    int readInt(const io::JsonValue &src, const std::string &key, int defVal) {
      const auto *value = src.find(key);
      return value ? value->asInt(defVal) : defVal;
    }

    bool readBool(const io::JsonValue &src, const std::string &key, bool defVal) {
      const auto *value = src.find(key);
      return value ? value->asBool(defVal) : defVal;
    }

    std::string readString(const io::JsonValue &src, const std::string &key, const std::string &defVal) {
      const auto *value = src.find(key);
      return value ? value->asString(defVal) : defVal;
    }

    void serializeEntity(std::ostringstream &ss, const SceneEntity &e, int level) {
      ss << indent(level) << "{\n";
      ss << indent(level + 1) << "\"id\": " << quoted(e.id) << ",\n";
      ss << indent(level + 1) << "\"name\": " << quoted(e.name) << ",\n";
      ss << indent(level + 1) << "\"type\": " << quoted(toString(e.type)) << ",\n";
      ss << indent(level + 1) << "\"transform\": {\n";
      ss << indent(level + 2) << "\"position\": "; writeVec3(ss, e.transform.position); ss << ",\n";
      ss << indent(level + 2) << "\"rotation\": "; writeVec3(ss, e.transform.rotation); ss << ",\n";
      ss << indent(level + 2) << "\"scale\": "; writeVec3(ss, e.transform.scale); ss << "\n";
      ss << indent(level + 1) << "}";

      if (e.sprite) {
        const auto &s = *e.sprite;
        ss << ",\n" << indent(level + 1) << "\"sprite\": {\n";
        ss << indent(level + 2) << "\"spriteMeta\": " << quoted(s.spriteMeta) << ",\n";
        ss << indent(level + 2) << "\"spriteMetaGuid\": " << quoted(s.spriteMetaGuid) << ",\n";
        ss << indent(level + 2) << "\"state\": " << quoted(s.state) << ",\n";
        ss << indent(level + 2) << "\"billboard\": " << quoted(toString(s.billboard)) << ",\n";
        ss << indent(level + 2) << "\"layer\": " << s.layer << "\n";
        ss << indent(level + 1) << "}";
      }

      if (e.mesh) {
        const auto &m = *e.mesh;
        ss << ",\n" << indent(level + 1) << "\"mesh\": {\n";
        ss << indent(level + 2) << "\"model\": " << quoted(m.model) << ",\n";
        ss << indent(level + 2) << "\"modelGuid\": " << quoted(m.modelGuid) << ",\n";
        ss << indent(level + 2) << "\"material\": " << quoted(m.material) << ",\n";
        ss << indent(level + 2) << "\"materialGuid\": " << quoted(m.materialGuid);
        if (!m.nodeOverrides.empty()) {
          ss << ",\n" << indent(level + 2) << "\"nodeOverrides\": [\n";
          for (std::size_t i = 0; i < m.nodeOverrides.size(); ++i) {
            const auto &o = m.nodeOverrides[i];
            ss << indent(level + 3) << "{\n";
            ss << indent(level + 4) << "\"node\": " << o.node << ",\n";
            ss << indent(level + 4) << "\"position\": "; writeVec3(ss, o.transform.position); ss << ",\n";
            ss << indent(level + 4) << "\"rotation\": "; writeVec3(ss, o.transform.rotation); ss << ",\n";
            ss << indent(level + 4) << "\"scale\": "; writeVec3(ss, o.transform.scale); ss << "\n";
            ss << indent(level + 3) << "}";
            if (i + 1 < m.nodeOverrides.size()) {
              ss << ",";
            }
            ss << "\n";
          }
          ss << indent(level + 2) << "]\n";
          ss << indent(level + 1) << "}";
        } else {
          ss << "\n" << indent(level + 1) << "}";
        }
      }

      if (e.light) {
        const auto &l = *e.light;
        ss << ",\n" << indent(level + 1) << "\"light\": {\n";
        ss << indent(level + 2) << "\"kind\": " << quoted(toString(l.kind)) << ",\n";
        ss << indent(level + 2) << "\"color\": "; writeVec3(ss, l.color); ss << ",\n";
        ss << indent(level + 2) << "\"intensity\": " << l.intensity << ",\n";
        ss << indent(level + 2) << "\"range\": " << l.range << ",\n";
        ss << indent(level + 2) << "\"angle\": " << l.angle << "\n";
        ss << indent(level + 1) << "}";
      }

      if (e.camera) {
        const auto &c = *e.camera;
        ss << ",\n" << indent(level + 1) << "\"camera\": {\n";
        ss << indent(level + 2) << "\"projection\": " << quoted(c.projection) << ",\n";
        ss << indent(level + 2) << "\"fov\": " << c.fov << ",\n";
        ss << indent(level + 2) << "\"orthoHeight\": " << c.orthoHeight << ",\n";
        ss << indent(level + 2) << "\"near\": " << c.nearPlane << ",\n";
        ss << indent(level + 2) << "\"far\": " << c.farPlane << ",\n";
        ss << indent(level + 2) << "\"active\": " << (c.active ? "true" : "false") << "\n";
        ss << indent(level + 1) << "}";
      }

      ss << "\n" << indent(level) << "}";
    }

  } // namespace

  bool SceneSerializer::saveToFile(const Scene &scene, const std::string &path) {
    std::ostringstream ss;
    ss << "{\n";
    ss << indent(1) << "\"version\": " << scene.version << ",\n";
    ss << indent(1) << "\"resources\": {\n";
    ss << indent(2) << "\"basePath\": " << quoted(scene.resources.basePath) << ",\n";
    ss << indent(2) << "\"sprites\": " << quoted(scene.resources.spritePath) << ",\n";
    ss << indent(2) << "\"models\": " << quoted(scene.resources.modelPath) << ",\n";
    ss << indent(2) << "\"materials\": " << quoted(scene.resources.materialPath) << "\n";
    ss << indent(1) << "},\n";
    ss << indent(1) << "\"entities\": [\n";
    for (std::size_t i = 0; i < scene.entities.size(); ++i) {
      serializeEntity(ss, scene.entities[i], 2);
      if (i + 1 < scene.entities.size()) {
        ss << ",";
      }
      ss << "\n";
    }
    ss << indent(1) << "]\n";
    ss << "}\n";
    return writeStringToFile(path, ss.str());
  }

  bool SceneSerializer::loadFromFile(const std::string &path, Scene &outScene) {
    const std::string content = readFileToString(path);
    if (content.empty()) return false;

    io::JsonValue root;
    if (!io::parseJson(content, root) || !root.isObject()) {
      return false;
    }

    outScene.version = readInt(root, "version", 1);
    if (const auto *resources = root.find("resources")) {
      outScene.resources.basePath = readString(*resources, "basePath", "");
      outScene.resources.spritePath = readString(*resources, "sprites", "");
      outScene.resources.modelPath = readString(*resources, "models", "");
      outScene.resources.materialPath = readString(*resources, "materials", "");
    }

    const auto *entities = root.find("entities");
    const auto *entityArray = entities ? entities->asArray() : nullptr;
    outScene.entities.clear();
    if (!entityArray) {
      return true;
    }

    for (const auto &entityJson : *entityArray) {
      if (!entityJson.isObject()) {
        continue;
      }
      SceneEntity e{};
      e.id = readString(entityJson, "id", "");
      e.name = readString(entityJson, "name", "");
      e.type = entityTypeFromString(readString(entityJson, "type", "mesh"));
      if (const auto *transform = entityJson.find("transform")) {
        e.transform.position = readVec3(*transform, "position", e.transform.position);
        e.transform.rotation = readVec3(*transform, "rotation", e.transform.rotation);
        e.transform.scale = readVec3(*transform, "scale", e.transform.scale);
      }

      if (const auto *sprite = entityJson.find("sprite")) {
        SpriteComponent sc{};
        sc.spriteMeta = readString(*sprite, "spriteMeta", sc.spriteMeta);
        sc.spriteMetaGuid = readString(*sprite, "spriteMetaGuid", sc.spriteMetaGuid);
        sc.state = readString(*sprite, "state", sc.state);
        sc.billboard = billboardFromString(readString(*sprite, "billboard", toString(sc.billboard)));
        sc.layer = readInt(*sprite, "layer", sc.layer);
        e.sprite = sc;
      }

      if (const auto *mesh = entityJson.find("mesh")) {
        MeshComponent mc{};
        mc.model = readString(*mesh, "model", mc.model);
        mc.modelGuid = readString(*mesh, "modelGuid", mc.modelGuid);
        mc.material = readString(*mesh, "material", mc.material);
        mc.materialGuid = readString(*mesh, "materialGuid", mc.materialGuid);
        const auto *nodeOverrides = mesh->find("nodeOverrides");
        const auto *overrideArray = nodeOverrides ? nodeOverrides->asArray() : nullptr;
        if (overrideArray) {
          for (const auto &overrideJson : *overrideArray) {
            if (!overrideJson.isObject()) continue;
            MeshComponent::NodeOverride ov{};
            ov.node = readInt(overrideJson, "node", ov.node);
            ov.transform.position = readVec3(overrideJson, "position", ov.transform.position);
            ov.transform.rotation = readVec3(overrideJson, "rotation", ov.transform.rotation);
            ov.transform.scale = readVec3(overrideJson, "scale", ov.transform.scale);
            if (ov.node >= 0) {
              mc.nodeOverrides.push_back(std::move(ov));
            }
          }
        }
        e.mesh = mc;
      }

      if (const auto *light = entityJson.find("light")) {
        LightComponent lc{};
        lc.kind = lightFromString(readString(*light, "kind", toString(lc.kind)));
        lc.color = readVec3(*light, "color", lc.color);
        lc.intensity = readFloat(*light, "intensity", lc.intensity);
        lc.range = readFloat(*light, "range", lc.range);
        lc.angle = readFloat(*light, "angle", lc.angle);
        e.light = lc;
      }

      if (const auto *camera = entityJson.find("camera")) {
        CameraComponent cc{};
        cc.projection = readString(*camera, "projection", cc.projection);
        cc.fov = readFloat(*camera, "fov", cc.fov);
        cc.orthoHeight = readFloat(*camera, "orthoHeight", cc.orthoHeight);
        cc.nearPlane = readFloat(*camera, "near", cc.nearPlane);
        cc.farPlane = readFloat(*camera, "far", cc.farPlane);
        cc.active = readBool(*camera, "active", cc.active);
        e.camera = cc;
      }

      outScene.entities.push_back(std::move(e));
    }

    return true;
  }

} // namespace lve
