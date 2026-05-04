#include "test_harness.hpp"

#include "Engine/Rendering/object_buffer.hpp"
#include "Engine/Rendering/render_assets.hpp"
#include "Engine/Scene/scene_system.hpp"

#include <memory>
#include <unordered_map>

namespace {

  class FakeTexture final : public lve::backend::RenderTexture {};

  class FakeMaterial final : public lve::backend::RenderMaterial {
  public:
    const lve::MaterialData &getData() const override { return data; }
    const std::string &getPath() const override { return path; }
    bool hasBaseColorTexture() const override { return false; }
    const lve::backend::RenderTexture *getBaseColorTexture() const override { return nullptr; }
    const lve::backend::RenderTexture *getNormalTexture() const override { return nullptr; }
    const lve::backend::RenderTexture *getMetallicRoughnessTexture() const override { return nullptr; }
    const lve::backend::RenderTexture *getOcclusionTexture() const override { return nullptr; }
    const lve::backend::RenderTexture *getEmissiveTexture() const override { return nullptr; }
    bool applyData(
      const lve::MaterialData &newData,
      std::string *,
      const std::function<std::string(const std::string &)> &) override {
      data = newData;
      return true;
    }
    void setPath(const std::string &newPath) override { path = newPath; }

  private:
    lve::MaterialData data{};
    std::string path{};
  };

  class FakeModel final : public lve::backend::RenderModel {
  public:
    const std::vector<lve::backend::ModelNode> &getNodes() const override { return nodes; }
    const std::vector<lve::backend::ModelSubMesh> &getSubMeshes() const override { return subMeshes; }
    const std::vector<lve::backend::MaterialPathInfo> &getMaterialPathInfo() const override { return materialPaths; }
    std::string getDiffusePathForMaterialIndex(int) const override { return {}; }
    std::string getDiffusePathForSubMesh(const lve::backend::ModelSubMesh &) const override { return {}; }
    const lve::backend::RenderTexture *getDiffuseTextureForSubMesh(const lve::backend::ModelSubMesh &) const override {
      return nullptr;
    }
    bool hasAnyDiffuseTexture() const override { return false; }
    void computeNodeGlobals(
      const std::vector<glm::mat4> &,
      std::vector<glm::mat4> &outGlobals) const override {
      outGlobals.assign(nodes.size(), glm::mat4{1.f});
    }
    const lve::backend::ModelBoundingBox &getBoundingBox() const override { return bounds; }

  private:
    std::vector<lve::backend::ModelNode> nodes{};
    std::vector<lve::backend::ModelSubMesh> subMeshes{};
    std::vector<lve::backend::MaterialPathInfo> materialPaths{};
    lve::backend::ModelBoundingBox bounds{};
  };

  class FakeAssetFactory final : public lve::backend::RenderAssetFactory {
  public:
    std::shared_ptr<lve::backend::RenderModel> loadModel(
      const std::string &,
      const lve::backend::ModelLoadOptions & = {}) override {
      return std::make_shared<FakeModel>();
    }
    std::shared_ptr<lve::backend::RenderMaterial> loadMaterial(
      const std::string &path,
      std::string *,
      const std::function<std::string(const std::string &)> &) override {
      auto material = std::make_shared<FakeMaterial>();
      material->setPath(path);
      return material;
    }
    std::shared_ptr<lve::backend::RenderMaterial> createMaterial() override {
      return std::make_shared<FakeMaterial>();
    }
    bool saveMaterial(const std::string &, const lve::MaterialData &, std::string *) override {
      return true;
    }
    std::shared_ptr<lve::backend::RenderTexture> loadTexture(
      const std::string &path,
      const lve::backend::TextureLoadOptions & = {}) override {
      auto it = textures.find(path);
      if (it != textures.end()) {
        return it->second;
      }
      auto texture = std::make_shared<FakeTexture>();
      textures[path] = texture;
      return texture;
    }
    std::shared_ptr<lve::backend::RenderTexture> getDefaultTexture() override {
      return loadTexture("default");
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<lve::backend::RenderTexture>> textures{};
  };

  class FakeObjectBufferPool final : public lve::backend::ObjectBufferPool {
  public:
    lve::backend::BufferInfo getBufferInfo(int, std::size_t) const override { return {}; }
    void writeToIndex(const void *, std::size_t) override {}
    void flush() override {}
  };

  lve::Scene makeIdScene() {
    lve::Scene scene{};

    lve::SceneEntity mesh{};
    mesh.id = "obj_7";
    mesh.name = "Mesh Seven";
    mesh.type = lve::EntityType::Mesh;
    mesh.mesh = lve::MeshComponent{};
    mesh.mesh->model = "Assets/models/colored_cube.obj";
    scene.entities.push_back(mesh);

    lve::SceneEntity sprite{};
    sprite.id = "obj_4";
    sprite.name = "Sprite Four";
    sprite.type = lve::EntityType::Sprite;
    sprite.sprite = lve::SpriteComponent{};
    sprite.sprite->spriteMeta = "Assets/textures/characters/missing_test.json";
    scene.entities.push_back(sprite);

    lve::SceneEntity light{};
    light.id = "obj_2";
    light.name = "Light Two";
    light.type = lve::EntityType::Light;
    light.transform.scale = {3.f, 3.f, 3.f};
    light.light = lve::LightComponent{};
    light.light->intensity = 5.f;
    light.light->color = {0.25f, 0.5f, 0.75f};
    scene.entities.push_back(light);

    lve::SceneEntity camera{};
    camera.id = "obj_11";
    camera.name = "Camera Eleven";
    camera.type = lve::EntityType::Camera;
    camera.camera = lve::CameraComponent{};
    camera.camera->active = true;
    scene.entities.push_back(camera);

    return scene;
  }

  void testSceneImportRestoresObjectIds() {
    FakeAssetFactory assets{};
    auto buffers = std::make_unique<FakeObjectBufferPool>();
    lve::SceneSystem sceneSystem{assets, std::move(buffers)};

    sceneSystem.importSceneSnapshot(makeIdScene(), std::nullopt);

    test::require(sceneSystem.findObject(7) != nullptr, "mesh id was not restored");
    test::require(sceneSystem.findObject(4) != nullptr, "sprite id was not restored");
    test::require(sceneSystem.findObject(2) != nullptr, "light id was not restored");
    test::require(sceneSystem.findObject(11) != nullptr, "camera id was not restored");

    const auto *light = sceneSystem.findObject(2);
    test::require(light && light->pointLight, "restored light is missing point light component");
    test::require(test::near(light->pointLight->lightIntensity, 5.f), "light intensity was not restored");

    const auto *camera = sceneSystem.findObject(11);
    test::require(camera && camera->camera && camera->camera->active, "active camera was not restored");
  }

} // namespace

int main() {
  return test::runSuite("SceneSystemTests", testSceneImportRestoresObjectIds);
}
