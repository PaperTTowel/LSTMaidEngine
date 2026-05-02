#pragma once

#include "Game/VisualNovel/scenario.hpp"

#include <string>

namespace lve::game::vn::ui {

  struct VisualNovelOverlayState {
    const ScenarioCommand *command{nullptr};
    const DialogueLine *dialogueLine{nullptr};
    std::string statusText;
    bool complete{false};
  };

  int drawVisualNovelOverlay(const VisualNovelOverlayState &state);

} // namespace lve::game::vn::ui
