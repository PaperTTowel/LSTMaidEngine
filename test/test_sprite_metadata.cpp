#include "test_harness.hpp"

#include "utils/sprite_metadata.hpp"

namespace {

  void testSpriteMetadata() {
    const auto path = test::outputDir("sprite_metadata") / "sprite_metadata.json";
    test::writeTextFile(path, R"json({
      "cols": 8,
      "rows": 2,
      "size": [32, 48],
      "pivot": [0.5, 1.0],
      "states": {
        "idle": {
          "texture": "Assets/textures/idle.png",
          "row": 0,
          "frames": 4,
          "fps": 8,
          "loop": true
        },
        "attack": {
          "texture": "Assets/textures/attack.png",
          "row": 1,
          "frames": 6,
          "frameDuration": 0.2,
          "loop": false,
          "cols": 6
        }
      }
    })json");

    lve::SpriteMetadata metadata{};
    test::require(lve::loadSpriteMetadata(path.string(), metadata), "failed to load sprite metadata");
    test::require(metadata.atlasCols == 8, "sprite atlas cols mismatch");
    test::require(metadata.atlasRows == 2, "sprite atlas rows mismatch");
    test::require(test::near(metadata.size.x, 32.f) && test::near(metadata.size.y, 48.f), "sprite size mismatch");
    test::require(metadata.states.size() == 2, "sprite state count mismatch");
    test::require(metadata.states.at("idle").frameCount == 4, "sprite idle frame count mismatch");
    test::require(test::near(metadata.states.at("idle").frameDuration, 0.125f), "sprite fps duration mismatch");
    test::require(!metadata.states.at("attack").loop, "sprite attack loop mismatch");
    test::require(metadata.states.at("attack").atlasCols == 6, "sprite state cols override mismatch");

    test::writeTextFile(path, "{\"states\":");
    test::require(!lve::loadSpriteMetadata(path.string(), metadata), "invalid sprite metadata should fail");
  }

} // namespace

int main() {
  return test::runSuite("SpriteMetadataTests", testSpriteMetadata);
}
