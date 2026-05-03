#include "test_harness.hpp"

#include "Engine/IO/json.hpp"

#include <string>

namespace {

  void testJsonParser() {
    const std::string content = R"json({
      "name": "maid \"core\"",
      "enabled": true,
      "values": [1, 2.5, -3],
      "nested": {
        "path": "Assets/textures/player.png"
      }
    })json";

    lve::io::JsonValue root;
    std::string error;
    test::require(lve::io::parseJson(content, root, &error), "json parse failed: " + error);
    test::require(root.isObject(), "json root should be an object");
    test::require(root.find("name")->asString() == "maid \"core\"", "json string escape mismatch");
    test::require(root.find("enabled")->asBool(false), "json bool mismatch");

    const auto *values = root.find("values")->asArray();
    test::require(values && values->size() == 3, "json array size mismatch");
    test::require((*values)[0].asInt() == 1, "json int array value mismatch");
    test::require(test::near(static_cast<float>((*values)[1].asNumber()), 2.5f), "json float array value mismatch");
    test::require((*values)[2].asInt() == -3, "json negative array value mismatch");

    const auto *nested = root.find("nested");
    test::require(nested && nested->find("path")->asString() == "Assets/textures/player.png", "json nested string mismatch");

    lve::io::JsonValue invalid{};
    test::require(!lve::io::parseJson("{\"unterminated\": [1, 2}", invalid, &error), "invalid json should fail");
    test::require(!error.empty(), "invalid json should report an error");

    const std::string escaped = lve::io::escapeJsonString("line\n\"quoted\"\\path");
    test::require(escaped == "line\\n\\\"quoted\\\"\\\\path", "json string escaping mismatch");
  }

} // namespace

int main() {
  return test::runSuite("JsonTests", testJsonParser);
}
