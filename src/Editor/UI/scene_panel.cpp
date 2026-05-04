#include "scene_panel.hpp"

#include <imgui.h>
#include <cstdio>

namespace lve::editor {

  ScenePanelActions BuildScenePanel(ScenePanelState &state, bool *open) {
    ScenePanelActions actions{};
    if (open) {
      if (!ImGui::Begin("Scene", open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin("Scene");
    }
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s", state.path.c_str());
    if (ImGui::InputText("Path", buffer, sizeof(buffer))) {
      state.path = buffer;
    }
    ImGui::Text("Current: %s%s", state.currentPath.c_str(), state.dirty ? " *" : "");
    ImGui::Text("Status: %s", state.statusMessage.c_str());
    if (ImGui::Button("Save")) {
      actions.saveRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
      actions.loadRequested = true;
    }
    ImGui::End();
    return actions;
  }

} // namespace lve::editor
