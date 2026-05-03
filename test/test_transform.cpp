#include "test_harness.hpp"

#include "utils/game_object.hpp"

namespace {

  void testTransformMatrices() {
    lve::TransformComponent transform{};
    transform.translation = {1.f, 2.f, 3.f};
    transform.scale = {2.f, 4.f, 0.5f};

    const glm::mat4 model = transform.mat4();
    test::require(test::near(model[3][0], 1.f), "model translation x mismatch");
    test::require(test::near(model[3][1], 2.f), "model translation y mismatch");
    test::require(test::near(model[3][2], 3.f), "model translation z mismatch");
    test::require(test::near(model[0][0], 2.f), "model scale x mismatch");
    test::require(test::near(model[1][1], 4.f), "model scale y mismatch");
    test::require(test::near(model[2][2], 0.5f), "model scale z mismatch");

    const glm::mat3 normal = transform.normalMatrix();
    test::require(test::near(normal[0][0], 0.5f), "normal inverse scale x mismatch");
    test::require(test::near(normal[1][1], 0.25f), "normal inverse scale y mismatch");
    test::require(test::near(normal[2][2], 2.f), "normal inverse scale z mismatch");
  }

} // namespace

int main() {
  return test::runSuite("TransformTests", testTransformMatrices);
}
