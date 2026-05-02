#pragma once

#include "Game/VisualNovel/scenario.hpp"

#include <cstddef>
#include <string>

namespace lve::game::vn {

  class VisualNovelSystem {
  public:
    bool setScenario(Scenario scenario, std::string *outError = nullptr);
    bool loadScenario(const std::string &path, std::string *outError = nullptr);
    void reset();

    const ScenarioCommand *currentCommand() const;
    bool advance(std::string *outError = nullptr);
    bool choose(std::size_t choiceIndex, std::string *outError = nullptr);
    bool goToNode(const std::string &targetNode, std::string *outError = nullptr);

    const std::string &currentNodeId() const { return nodeId; }
    std::size_t currentCommandIndex() const { return commandIndex; }
    bool isLoaded() const { return loaded; }
    bool isComplete() const { return complete; }
    bool isWaitingForChoice() const;

  private:
    const ScenarioNode *findCurrentNode() const;
    bool jumpToNode(const std::string &targetNode, std::string *outError);

    Scenario scenario{};
    std::string nodeId;
    std::size_t commandIndex{0};
    bool loaded{false};
    bool complete{false};
  };

} // namespace lve::game::vn
