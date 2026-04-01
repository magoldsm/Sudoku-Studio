#include "ui_helpers.h"

#include <array>
#include <string>

#include "imgui.h"
#include "ui_constants.h"
#include "sudoku_core.h"

namespace sudoku {
// Extracted from main.cpp during modular refactor.
// Input/mode helpers and technique panel rendering.
int ReadDigitPress() {
  for (int d = 1; d <= 9; ++d) {
    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + (d - 1))) ||
        ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad1 + (d - 1)))) {
      return d;
    }
  }
  return 0;
}

bool IsClearPressed() {
  return ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0) ||
         ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete);
}

const char* ModeName(InputMode mode) {
  switch (mode) {
    case InputMode::kDigit:
      return "Digit";
    case InputMode::kPencil:
      return "Pencil";
    case InputMode::kColor:
      return "Color";
  }
  return "Unknown";
}

const char* DifficultyName(Difficulty difficulty) {
  switch (difficulty) {
    case Difficulty::kSimple:
      return "Simple";
    case Difficulty::kEasy:
      return "Easy";
    case Difficulty::kMild:
      return "Mild";
    case Difficulty::kModerate:
      return "Moderate";
    case Difficulty::kHard:
      return "Hard";
    case Difficulty::kVeryHard:
      return "Very Hard";
    case Difficulty::kFiendish:
      return "Fiendish";
    case Difficulty::kDiabolical:
      return "Diabolical";
  }
  return "Unknown";
}

void DrawTechniquePanel(const Hint& currentHint) {
  struct TechniqueEntry {
    const char* name;
    int level;
  };

  static const std::array<TechniqueEntry, 18> kTechniques = {{
      {"Naked Single", 1},
      {"Hidden Single", 1},
      {"Pointing Pair/Triple", 2},
      {"Box/Line Reduction", 2},
      {"Naked Pair", 2},
      {"Hidden Pair", 2},
      {"Naked Triple", 3},
      {"Hidden Triple", 3},
      {"Naked Quad", 4},
      {"Hidden Quad", 4},
      {"Block/Block Interaction", 4},
      {"X-Wing", 5},
      {"Unique Rectangle", 5},
      {"Y-Wing", 5},
      {"Simple Colouring", 6},
      {"Swordfish", 6},
      {"XYZ-Wing", 6},
      {"XY-Chain", 7},
  }};

  ImGui::BeginChild("TechniquePanel", ImVec2(0.0f, 220.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::TextUnformatted("Technique Panel");
  ImGui::SameLine();
  ImGui::TextDisabled("(Sadman-style order)");
  ImGui::Separator();

  const bool hasActive = !currentHint.techniqueName.empty() &&
                         currentHint.techniqueName != "No supported Sadman hint available";

  if (hasActive) {
    ImGui::Text("Active: %s", currentHint.techniqueName.c_str());
  } else {
    ImGui::TextDisabled("Active: none");
  }
  ImGui::Spacing();

  for (const TechniqueEntry& entry : kTechniques) {
    const bool isActive = hasActive && currentHint.techniqueName == entry.name;
    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.38f, 0.68f, 1.0f));
      ImGui::Text("[%d] %s", entry.level, entry.name);
      ImGui::PopStyleColor();
    } else {
      ImGui::TextDisabled("[%d] %s", entry.level, entry.name);
    }
  }

  ImGui::TextDisabled("[7] Jellyfish");
  ImGui::TextDisabled("[8] Forcing Chains");
  ImGui::EndChild();
}

void DrawColorTagPanel(int& activeColor, InputMode mode,
                       const std::array<ImU32, 10>& tagColors) {
  ImGui::BeginChild("TagPanel", ImVec2(kTagPanelWidth, kBoardSize + 18.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

  ImGui::TextUnformatted("Color Tags");
  ImGui::Spacing();
  for (int tag = 1; tag <= 9; ++tag) {
    ImGui::PushID(tag);
    if (mode == InputMode::kColor && activeColor == tag) {
      ImGui::PushStyleColor(ImGuiCol_Button, kColorActiveButtonOverride);
    } else {
      const ImVec4 c = ImGui::ColorConvertU32ToFloat4(tagColors[tag]);
      ImGui::PushStyleColor(ImGuiCol_Button, c);
    }

    if (ImGui::Button(std::to_string(tag).c_str(), ImVec2(kTagButtonWidth, kTagButtonHeight))) {
      activeColor = tag;
    }
    ImGui::PopStyleColor();
    ImGui::PopID();
  }

  ImGui::EndChild();
}

void DrawGridLines(ImDrawList* draw, const ImVec2& origin, const ImVec2& boardMax) {
  for (int i = 0; i <= kGridSize; ++i) {
    const float x = origin.x + i * kCellSize;
    const float y = origin.y + i * kCellSize;
    const float thickness = (i % 3 == 0) ? kGridThickLineWidth : kGridThinLineWidth;
    const ImU32 color = (i % 3 == 0) ? kGridThickLineColor : kGridThinLineColor;

    draw->AddLine(ImVec2(x, origin.y), ImVec2(x, boardMax.y), color, thickness);
    draw->AddLine(ImVec2(origin.x, y), ImVec2(boardMax.x, y), color, thickness);
  }
}

}  // namespace sudoku
