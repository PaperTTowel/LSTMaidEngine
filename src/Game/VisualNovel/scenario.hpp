#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace lve::game::vn {

  enum class ScenarioCommandType {
    Background,
    ShowCharacter,
    HideCharacter,
    Say,
    DialogueFile,
    Choice,
    Goto,
    Battle,
    PlayBgm,
    PlaySe,
    SetFlag,
    Unknown
  };

  struct ChoiceOption {
    std::string text;
    std::string targetNode;
  };

  struct ScenarioCommand {
    ScenarioCommandType type{ScenarioCommandType::Unknown};
    std::string image;
    std::string character;
    std::string pose;
    std::string slot;
    std::string speaker;
    std::string text;
    std::string file;
    std::string targetNode;
    std::string enemy;
    std::string winNode;
    std::string loseNode;
    std::string flag;
    bool flagValue{false};
    std::vector<ChoiceOption> choices;
  };

  struct ScenarioNode {
    std::string id;
    std::vector<ScenarioCommand> commands;
  };

  struct Scenario {
    std::string startNode;
    std::unordered_map<std::string, ScenarioNode> nodes;
  };

  struct DialogueLine {
    std::string speaker;
    std::string text;
    std::string character;
    std::string pose;
    std::string slot;
  };

} // namespace lve::game::vn
