#include "test_harness.hpp"

#include "Engine/IO/material_io.hpp"

#include <string>

namespace {

  void testMaterialIo() {
    lve::MaterialData data{};
    data.version = 3;
    data.name = "Test Material";
    data.textures.baseColor = "Assets\\textures\\base.png";
    data.textures.normal = "Assets/textures/normal.png";
    data.factors.baseColor = {0.25f, 0.5f, 0.75f, 1.f};
    data.factors.metallic = 0.3f;
    data.factors.roughness = 0.8f;
    data.factors.emissive = {1.f, 2.f, 3.f};

    const auto path = test::outputDir("material") / "material_roundtrip.mat";
    std::string error;
    test::require(lve::saveMaterialToFile(path.string(), data, &error), "failed to save material: " + error);

    lve::MaterialData loaded{};
    test::require(lve::loadMaterialDataFromFile(path.string(), loaded, &error), "failed to load material: " + error);
    test::require(loaded.version == 3, "material version mismatch");
    test::require(loaded.name == "Test Material", "material name mismatch");
    test::require(loaded.textures.baseColor == "Assets/textures/base.png", "material base texture path normalization mismatch");
    test::require(test::near(loaded.factors.baseColor.b, 0.75f), "material base color factor mismatch");
    test::require(test::near(loaded.factors.metallic, 0.3f), "material metallic factor mismatch");

    const auto invalidPath = test::outputDir("material") / "invalid_material.mat";
    test::writeTextFile(invalidPath, "{\"name\": \"broken\",");
    test::require(!lve::loadMaterialDataFromFile(invalidPath.string(), loaded, &error), "invalid material json should fail");
    test::require(!error.empty(), "invalid material json should report an error");
  }

} // namespace

int main() {
  return test::runSuite("MaterialIoTests", testMaterialIo);
}
