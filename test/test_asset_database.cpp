#include "test_harness.hpp"

#include "Engine/Assets/asset_database.hpp"

namespace {

  void testAssetDatabase() {
    const auto root = test::outputDir("asset_database");
    std::filesystem::create_directories(root / "textures");

    const auto scenePath = root / "scene.json";
    const auto spritePath = root / "sprite.json";
    const auto texturePath = root / "textures" / "base.png";
    test::writeTextFile(scenePath, R"json({"version": 1, "entities": []})json");
    test::writeTextFile(spritePath, R"json({"states": {"idle": {"frames": 1}}})json");
    test::writeTextFile(texturePath, "not a real png, type detection only uses extension");

    lve::AssetDatabase database{root.generic_string()};
    database.initialize();

    const std::string sceneAssetPath = scenePath.generic_string();
    const std::string spriteAssetPath = spritePath.generic_string();
    const std::string textureAssetPath = texturePath.generic_string();

    const auto *sceneMeta = database.getMetaForPath(sceneAssetPath);
    const auto *spriteMeta = database.getMetaForPath(spriteAssetPath);
    const auto *textureMeta = database.getMetaForPath(textureAssetPath);

    test::require(sceneMeta && sceneMeta->type == lve::AssetType::Scene, "asset database scene type mismatch");
    test::require(spriteMeta && spriteMeta->type == lve::AssetType::SpriteMeta, "asset database sprite type mismatch");
    test::require(textureMeta && textureMeta->type == lve::AssetType::Texture, "asset database texture type mismatch");
    test::require(!database.getGuidForPath(sceneAssetPath).empty(), "asset database scene guid missing");
    test::require(database.resolveAssetPath(sceneAssetPath) == sceneAssetPath, "asset database resolve path mismatch");
  }

} // namespace

int main() {
  return test::runSuite("AssetDatabaseTests", testAssetDatabase);
}
