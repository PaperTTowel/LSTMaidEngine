#pragma once

#include <string>

namespace lve::editor {

  struct ScenePanelState {
    std::string path{"Assets/scene.json"};
    std::string currentPath{"Assets/scene.json"};
    std::string statusMessage{"Ready"};
    bool dirty{false};
    bool loadConfirmRequested{false};
    bool loadConfirmOpen{false};
  };

  struct ScenePanelActions {
    bool saveRequested{false};
    bool loadRequested{false};
  };

  ScenePanelActions BuildScenePanel(ScenePanelState &state, bool *open);

} // namespace lve::editor
