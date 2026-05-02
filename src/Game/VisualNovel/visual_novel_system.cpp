#include "Game/VisualNovel/visual_novel_system.hpp"

#include "Game/VisualNovel/scenario_loader.hpp"

#include <utility>

namespace lve::game::vn {
  namespace {
    void setError(std::string *outError, const std::string &message) {
      if (outError) {
        *outError = message;
      }
    }
  } // namespace

  bool VisualNovelSystem::setScenario(Scenario nextScenario, std::string *outError) {
    if (nextScenario.startNode.empty()) {
      setError(outError, "scenario missing start node");
      return false;
    }
    if (nextScenario.nodes.find(nextScenario.startNode) == nextScenario.nodes.end()) {
      setError(outError, "start node not found: " + nextScenario.startNode);
      return false;
    }

    scenario = std::move(nextScenario);
    loaded = true;
    reset();
    return true;
  }

  bool VisualNovelSystem::loadScenario(const std::string &path, std::string *outError) {
    Scenario nextScenario{};
    if (!ScenarioLoader::loadScenarioFromFile(path, nextScenario, outError)) {
      return false;
    }
    return setScenario(std::move(nextScenario), outError);
  }

  void VisualNovelSystem::reset() {
    nodeId = scenario.startNode;
    commandIndex = 0;
    complete = !loaded;
  }

  const ScenarioNode *VisualNovelSystem::findCurrentNode() const {
    auto it = scenario.nodes.find(nodeId);
    if (it == scenario.nodes.end()) {
      return nullptr;
    }
    return &it->second;
  }

  const ScenarioCommand *VisualNovelSystem::currentCommand() const {
    if (!loaded || complete) {
      return nullptr;
    }
    const ScenarioNode *node = findCurrentNode();
    if (!node || commandIndex >= node->commands.size()) {
      return nullptr;
    }
    return &node->commands[commandIndex];
  }

  bool VisualNovelSystem::jumpToNode(const std::string &targetNode, std::string *outError) {
    if (targetNode.empty()) {
      setError(outError, "target node is empty");
      return false;
    }
    if (scenario.nodes.find(targetNode) == scenario.nodes.end()) {
      setError(outError, "target node not found: " + targetNode);
      return false;
    }
    nodeId = targetNode;
    commandIndex = 0;
    complete = false;
    return true;
  }

  bool VisualNovelSystem::advance(std::string *outError) {
    const ScenarioCommand *command = currentCommand();
    if (!command) {
      complete = true;
      return false;
    }
    if (command->type == ScenarioCommandType::Choice) {
      return false;
    }
    if (command->type == ScenarioCommandType::Goto) {
      return jumpToNode(command->targetNode, outError);
    }

    ++commandIndex;
    const ScenarioNode *node = findCurrentNode();
    if (!node || commandIndex >= node->commands.size()) {
      complete = true;
    }
    return true;
  }

  bool VisualNovelSystem::choose(std::size_t choiceIndex, std::string *outError) {
    const ScenarioCommand *command = currentCommand();
    if (!command || command->type != ScenarioCommandType::Choice) {
      setError(outError, "current command is not a choice");
      return false;
    }
    if (choiceIndex >= command->choices.size()) {
      setError(outError, "choice index out of range");
      return false;
    }
    return jumpToNode(command->choices[choiceIndex].targetNode, outError);
  }

  bool VisualNovelSystem::goToNode(const std::string &targetNode, std::string *outError) {
    return jumpToNode(targetNode, outError);
  }

  bool VisualNovelSystem::isWaitingForChoice() const {
    const ScenarioCommand *command = currentCommand();
    return command && command->type == ScenarioCommandType::Choice;
  }

} // namespace lve::game::vn
