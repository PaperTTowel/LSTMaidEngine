#include "Game/VisualNovel/visual_novel_overlay.hpp"

#include <imgui.h>

#include <cstddef>

namespace lve::game::vn::ui {
  namespace {
    const char *speakerOf(const VisualNovelOverlayState &state) {
      if (state.dialogueLine && !state.dialogueLine->speaker.empty()) {
        return state.dialogueLine->speaker.c_str();
      }
      if (state.command && !state.command->speaker.empty()) {
        return state.command->speaker.c_str();
      }
      return "";
    }

    const char *textOf(const VisualNovelOverlayState &state) {
      if (state.dialogueLine) {
        return state.dialogueLine->text.c_str();
      }
      if (state.command) {
        return state.command->text.c_str();
      }
      return "";
    }
  } // namespace

  int drawVisualNovelOverlay(const VisualNovelOverlayState &state) {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!viewport) {
      return -1;
    }

    const float panelMargin = 28.f;
    const float panelHeight = 150.f;
    const ImVec2 panelPos{
      viewport->Pos.x + panelMargin,
      viewport->Pos.y + viewport->Size.y - panelHeight - panelMargin};
    const ImVec2 panelSize{
      viewport->Size.x - (panelMargin * 2.f),
      panelHeight};

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("VisualNovel.Dialogue", nullptr, flags);

    const char *speaker = speakerOf(state);
    if (speaker[0] != '\0') {
      ImGui::TextUnformatted(speaker);
      ImGui::Separator();
    }

    if (state.complete) {
      ImGui::TextWrapped("Scenario complete.");
    } else {
      ImGui::TextWrapped("%s", textOf(state));
    }

    if (!state.statusText.empty()) {
      ImGui::SetCursorPosY(panelSize.y - 28.f);
      ImGui::TextDisabled("%s", state.statusText.c_str());
    }

    ImGui::End();

    int selectedChoice = -1;
    if (state.command && state.command->type == ScenarioCommandType::Choice) {
      const float choiceWidth = 420.f;
      const float choiceHeight = 46.f;
      const float totalHeight = choiceHeight * static_cast<float>(state.command->choices.size());
      ImGui::SetNextWindowPos(
        ImVec2{
          viewport->Pos.x + (viewport->Size.x - choiceWidth) * 0.5f,
          viewport->Pos.y + (viewport->Size.y - totalHeight) * 0.42f},
        ImGuiCond_Always);
      ImGui::SetNextWindowSize(
        ImVec2{choiceWidth, totalHeight + 22.f},
        ImGuiCond_Always);
      ImGui::Begin("VisualNovel.Choices", nullptr, flags);
      for (int i = 0; i < static_cast<int>(state.command->choices.size()); ++i) {
        const auto &choice = state.command->choices[static_cast<std::size_t>(i)];
        if (ImGui::Button(choice.text.c_str(), ImVec2{-1.f, 36.f})) {
          selectedChoice = i;
        }
      }
      ImGui::End();
    }

    return selectedChoice;
  }

} // namespace lve::game::vn::ui
