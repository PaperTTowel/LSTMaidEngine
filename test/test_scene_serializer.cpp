#include "test_harness.hpp"

#include "Engine/scene.hpp"

namespace {

  void testSceneSerializer() {
    lve::Scene scene{};
    scene.version = 1;
    scene.resources.basePath = "Assets/";
    scene.resources.spritePath = "Assets/textures/";
    scene.resources.modelPath = "Assets/models/";
    scene.resources.materialPath = "Assets/materials/";

    lve::SceneEntity mesh{};
    mesh.id = "obj_1";
    mesh.name = "Mesh \"One\"";
    mesh.type = lve::EntityType::Mesh;
    mesh.transform.position = {1.f, 2.f, 3.f};
    mesh.transform.rotation = {0.1f, 0.2f, 0.3f};
    mesh.transform.scale = {2.f, 2.f, 2.f};
    lve::MeshComponent meshComponent{};
    meshComponent.model = "Assets/models/colored_cube.obj";
    meshComponent.material = "Assets/materials/test.mat";
    lve::MeshComponent::NodeOverride override{};
    override.node = 2;
    override.transform.position = {4.f, 5.f, 6.f};
    override.transform.scale = {1.f, 2.f, 3.f};
    meshComponent.nodeOverrides.push_back(override);
    mesh.mesh = meshComponent;
    scene.entities.push_back(mesh);

    lve::SceneEntity sprite{};
    sprite.id = "obj_2";
    sprite.name = "Sprite";
    sprite.type = lve::EntityType::Sprite;
    lve::SpriteComponent spriteComponent{};
    spriteComponent.spriteMeta = "Assets/textures/characters/player.json";
    spriteComponent.state = "idle";
    spriteComponent.billboard = lve::BillboardKind::Spherical;
    spriteComponent.layer = 3;
    sprite.sprite = spriteComponent;
    scene.entities.push_back(sprite);

    lve::SceneEntity light{};
    light.id = "obj_3";
    light.name = "Light";
    light.type = lve::EntityType::Light;
    lve::LightComponent lightComponent{};
    lightComponent.kind = lve::LightKind::Directional;
    lightComponent.color = {0.5f, 0.6f, 0.7f};
    lightComponent.intensity = 2.5f;
    light.light = lightComponent;
    scene.entities.push_back(light);

    lve::SceneEntity camera{};
    camera.id = "obj_4";
    camera.name = "Camera";
    camera.type = lve::EntityType::Camera;
    camera.camera = lve::CameraComponent{};
    camera.camera->active = true;
    camera.camera->fov = 70.f;
    scene.entities.push_back(camera);

    const auto path = test::outputDir("scene") / "scene_roundtrip.json";
    test::require(lve::SceneSerializer::saveToFile(scene, path.string()), "failed to save scene");

    lve::Scene loaded{};
    test::require(lve::SceneSerializer::loadFromFile(path.string(), loaded), "failed to load scene");
    test::require(loaded.entities.size() == 4, "scene entity count mismatch");
    test::require(loaded.entities[0].name == "Mesh \"One\"", "scene escaped name mismatch");
    test::require(loaded.entities[0].mesh.has_value(), "scene mesh component missing");
    test::require(loaded.entities[0].mesh->nodeOverrides.size() == 1, "scene node override count mismatch");
    test::require(loaded.entities[0].mesh->nodeOverrides[0].node == 2, "scene node override index mismatch");
    test::require(loaded.entities[1].sprite.has_value(), "scene sprite component missing");
    test::require(loaded.entities[1].sprite->billboard == lve::BillboardKind::Spherical, "scene sprite billboard mismatch");
    test::require(loaded.entities[1].sprite->layer == 3, "scene sprite layer mismatch");
    test::require(loaded.entities[2].light.has_value(), "scene light component missing");
    test::require(loaded.entities[2].light->kind == lve::LightKind::Directional, "scene light kind mismatch");
    test::require(test::near(loaded.entities[2].light->intensity, 2.5f), "scene light intensity mismatch");
    test::require(loaded.entities[3].camera.has_value(), "scene camera component missing");
    test::require(loaded.entities[3].camera->active, "scene camera active flag mismatch");

    test::writeTextFile(path, "{\"version\": 1}");
    test::require(lve::SceneSerializer::loadFromFile(path.string(), loaded), "scene without entities should still load");
    test::require(loaded.entities.empty(), "scene without entities should clear previous entities");
  }

} // namespace

int main() {
  return test::runSuite("SceneSerializerTests", testSceneSerializer);
}
