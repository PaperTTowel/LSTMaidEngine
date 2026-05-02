#include "Game/RPGBattle/battle_overlay.hpp"

#include <imgui.h>

namespace lve::game::battle::ui {

  bool drawBattleOverlay(const BattleSystem &battleSystem) {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!viewport) {
      return false;
    }

    ImDrawList *bg = ImGui::GetBackgroundDrawList(viewport);
    bg->AddRectFilled(
      viewport->Pos,
      ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
      IM_COL32(34, 28, 34, 255));

    ImGui::SetNextWindowPos(
      ImVec2{viewport->Pos.x + 32.f, viewport->Pos.y + 32.f},
      ImGuiCond_Always);
    ImGui::SetNextWindowSize(
      ImVec2{viewport->Size.x - 64.f, viewport->Size.y - 64.f},
      ImGuiCond_Always);
    ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoCollapse;

    bool attackPressed = false;
    ImGui::Begin("Battle", nullptr, flags);
    ImGui::TextUnformatted("Battle");
    ImGui::Separator();

    ImGui::TextUnformatted("Party");
    for (const auto &actor : battleSystem.party()) {
      ImGui::Text("%s  HP %d / %d", actor.data.displayName.c_str(), actor.hp, actor.data.maxHp);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Enemies");
    for (const auto &actor : battleSystem.enemies()) {
      ImGui::Text("%s  HP %d / %d", actor.data.displayName.c_str(), actor.hp, actor.data.maxHp);
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", battleSystem.lastLog().c_str());
    if (ImGui::Button("Attack", ImVec2{160.f, 36.f})) {
      attackPressed = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Space");
    ImGui::End();

    return attackPressed;
  }

} // namespace lve::game::battle::ui
