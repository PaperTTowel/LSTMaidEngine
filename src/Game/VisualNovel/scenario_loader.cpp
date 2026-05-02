#include "Game/VisualNovel/scenario_loader.hpp"

#include "Engine/path_utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <sstream>
#include <utility>

namespace lve::game::vn {
  namespace {
    void setError(std::string *outError, const std::string &message) {
      if (outError) {
        *outError = message;
      }
    }

    std::string readFileToString(const std::string &path) {
      std::ifstream file(pathutil::fromUtf8(path), std::ios::in | std::ios::binary);
      if (!file) {
        return {};
      }
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    std::string readStringMember(
      const rapidjson::Value &value,
      const char *name,
      const std::string &fallback = {}) {
      if (!value.IsObject() || !value.HasMember(name) || !value[name].IsString()) {
        return fallback;
      }
      return value[name].GetString();
    }

    bool readBoolMember(const rapidjson::Value &value, const char *name, bool fallback = false) {
      if (!value.IsObject() || !value.HasMember(name) || !value[name].IsBool()) {
        return fallback;
      }
      return value[name].GetBool();
    }

    ScenarioCommandType commandTypeFromString(const std::string &type) {
      if (type == "background") return ScenarioCommandType::Background;
      if (type == "show") return ScenarioCommandType::ShowCharacter;
      if (type == "hide") return ScenarioCommandType::HideCharacter;
      if (type == "say") return ScenarioCommandType::Say;
      if (type == "dialogue") return ScenarioCommandType::DialogueFile;
      if (type == "choice") return ScenarioCommandType::Choice;
      if (type == "goto") return ScenarioCommandType::Goto;
      if (type == "battle") return ScenarioCommandType::Battle;
      if (type == "playBgm") return ScenarioCommandType::PlayBgm;
      if (type == "playSe") return ScenarioCommandType::PlaySe;
      if (type == "setFlag") return ScenarioCommandType::SetFlag;
      return ScenarioCommandType::Unknown;
    }

    ScenarioCommand parseCommand(const rapidjson::Value &value) {
      ScenarioCommand command{};
      const std::string type = readStringMember(value, "type");
      command.type = commandTypeFromString(type);
      command.image = readStringMember(value, "image");
      command.character = readStringMember(value, "character");
      command.pose = readStringMember(value, "pose");
      command.slot = readStringMember(value, "slot");
      command.speaker = readStringMember(value, "speaker");
      command.text = readStringMember(value, "text");
      command.file = readStringMember(value, "file");
      command.targetNode = readStringMember(value, "target");
      if (command.targetNode.empty()) {
        command.targetNode = readStringMember(value, "goto");
      }
      command.enemy = readStringMember(value, "enemy");
      command.winNode = readStringMember(value, "win");
      command.loseNode = readStringMember(value, "lose");
      command.flag = readStringMember(value, "flag");
      command.flagValue = readBoolMember(value, "value");

      if (value.IsObject() && value.HasMember("choices") && value["choices"].IsArray()) {
        for (const auto &choiceValue : value["choices"].GetArray()) {
          if (!choiceValue.IsObject()) {
            continue;
          }
          ChoiceOption option{};
          option.text = readStringMember(choiceValue, "text");
          option.targetNode = readStringMember(choiceValue, "goto");
          if (option.targetNode.empty()) {
            option.targetNode = readStringMember(choiceValue, "target");
          }
          command.choices.push_back(std::move(option));
        }
      }

      return command;
    }
  } // namespace

  bool ScenarioLoader::loadScenarioFromFile(
    const std::string &path,
    Scenario &outScenario,
    std::string *outError) {
    outScenario = Scenario{};
    const std::string content = readFileToString(path);
    if (content.empty()) {
      setError(outError, "failed to read scenario file: " + path);
      return false;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
      setError(outError, std::string{"scenario json parse error: "} +
        rapidjson::GetParseError_En(doc.GetParseError()));
      return false;
    }
    if (!doc.IsObject()) {
      setError(outError, "scenario root must be an object");
      return false;
    }

    outScenario.startNode = readStringMember(doc, "start");
    if (outScenario.startNode.empty()) {
      setError(outError, "scenario missing start node");
      return false;
    }
    if (!doc.HasMember("nodes") || !doc["nodes"].IsObject()) {
      setError(outError, "scenario missing nodes object");
      return false;
    }

    for (auto nodeIt = doc["nodes"].MemberBegin(); nodeIt != doc["nodes"].MemberEnd(); ++nodeIt) {
      if (!nodeIt->name.IsString() || !nodeIt->value.IsArray()) {
        continue;
      }
      ScenarioNode node{};
      node.id = nodeIt->name.GetString();
      for (const auto &commandValue : nodeIt->value.GetArray()) {
        if (commandValue.IsObject()) {
          node.commands.push_back(parseCommand(commandValue));
        }
      }
      outScenario.nodes[node.id] = std::move(node);
    }

    if (outScenario.nodes.find(outScenario.startNode) == outScenario.nodes.end()) {
      setError(outError, "start node not found: " + outScenario.startNode);
      return false;
    }

    return true;
  }

  bool ScenarioLoader::loadDialogueFromFile(
    const std::string &path,
    std::vector<DialogueLine> &outLines,
    std::string *outError) {
    outLines.clear();
    const std::string content = readFileToString(path);
    if (content.empty()) {
      setError(outError, "failed to read dialogue file: " + path);
      return false;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
      setError(outError, std::string{"dialogue json parse error: "} +
        rapidjson::GetParseError_En(doc.GetParseError()));
      return false;
    }
    if (!doc.IsArray()) {
      setError(outError, "dialogue root must be an array");
      return false;
    }

    for (const auto &lineValue : doc.GetArray()) {
      if (!lineValue.IsObject()) {
        continue;
      }
      DialogueLine line{};
      line.speaker = readStringMember(lineValue, "speaker");
      line.text = readStringMember(lineValue, "text");
      line.character = readStringMember(lineValue, "character");
      line.pose = readStringMember(lineValue, "pose");
      line.slot = readStringMember(lineValue, "slot");
      outLines.push_back(std::move(line));
    }

    return true;
  }

} // namespace lve::game::vn
