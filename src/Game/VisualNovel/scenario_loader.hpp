#pragma once

#include "Game/VisualNovel/scenario.hpp"

#include <string>
#include <vector>

namespace lve::game::vn {

  class ScenarioLoader {
  public:
    static bool loadScenarioFromFile(
      const std::string &path,
      Scenario &outScenario,
      std::string *outError = nullptr);

    static bool loadDialogueFromFile(
      const std::string &path,
      std::vector<DialogueLine> &outLines,
      std::string *outError = nullptr);
  };

} // namespace lve::game::vn
