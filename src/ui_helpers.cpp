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

void DrawTechniquePanel(const Hint& currentHint, float width) {
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

  // If width is 0, use full available width; otherwise use specified width
  // Height matches board area (648 + 18 = 666) to avoid scrolling
  float panelWidth = (width > 0.0f) ? width : 0.0f;
  ImGui::BeginChild("TechniquePanel", ImVec2(panelWidth, 666.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::TextUnformatted("Technique Panel");
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

void DrawColorTagPanel(Grid& grid,
                       int& activeColor, InputMode& mode,
                       const std::array<ImU32, 10>& tagColors,
                       int& showPositionDigit, int& showEffectDigit,
                       bool& highlightPairs) {
  ImGui::BeginChild("TagPanel", ImVec2(kTagPanelWidth, kBoardSize + 18.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

  ImDrawList* draw = ImGui::GetWindowDrawList();

  // --- COLOR TAGS SECTION (1-9 buttons, compact size) ---
  constexpr float compactTagWidth = 100.0f;
  constexpr float compactTagHeight = 27.0f;

  for (int tag = 1; tag <= 9; ++tag) {
    ImGui::PushID(tag);
    const ImVec4 c = ImGui::ColorConvertU32ToFloat4(tagColors[tag]);
    ImGui::PushStyleColor(ImGuiCol_Button, c);

    if (ImGui::Button(std::to_string(tag).c_str(), ImVec2(compactTagWidth, compactTagHeight))) {
      activeColor = tag;
      mode = InputMode::kColor;  // Auto-switch to color mode when color is clicked
    }

    // Draw border around active color tag
    if (mode == InputMode::kColor && activeColor == tag) {
      ImVec2 buttonMin = ImGui::GetItemRectMin();
      ImVec2 buttonMax = ImGui::GetItemRectMax();
      constexpr ImU32 borderColor = IM_COL32(30, 100, 210, 255);  // Bright blue
      constexpr float borderThickness = 3.0f;
      draw->AddRect(buttonMin, buttonMax, borderColor, 0.0f, 0, borderThickness);
    }

    ImGui::PopStyleColor();
    ImGui::PopID();
  }

  ImGui::Separator();

  // --- DIGIT HIGHLIGHTING SECTION (compact size) ---
  constexpr ImU32 brightGreen = IM_COL32(0, 255, 0, 255);  // Bright green for highlighting
  constexpr ImU32 slashBlue = IM_COL32(30, 100, 210, 255);  // Blue for slashes
  constexpr float digitButtonWidth = 35.0f;
  constexpr float digitButtonHeight = 27.0f;

  for (int digit = 1; digit <= 9; ++digit) {
    ImGui::PushID(100 + digit);  // Offset ID to avoid collision with color tags

    // --- SHOW POSITION BUTTON (circle with digit) ---
    const ImVec2 buttonPos = ImGui::GetCursorScreenPos();
    const ImVec2 buttonSize(digitButtonWidth, digitButtonHeight);
    const ImVec2 buttonCenter(buttonPos.x + buttonSize.x * 0.5f, buttonPos.y + buttonSize.y * 0.5f);
    const float circleRadius = buttonSize.x * 0.35f;

    // Invisible button for click detection
    ImGui::InvisibleButton(("pos" + std::to_string(digit)).c_str(), buttonSize);
    if (ImGui::IsItemClicked()) {
      showPositionDigit = (showPositionDigit == digit) ? 0 : digit;
    }

    // Draw green circle
    draw->AddCircleFilled(buttonCenter, circleRadius, brightGreen);

    // Draw digit inside circle (black text, centered)
    const std::string digitStr = std::to_string(digit);
    ImFont* font = ImGui::GetFont();
    const ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, digitStr.c_str());
    const ImVec2 textPos(buttonCenter.x - textSize.x * 0.5f, buttonCenter.y - textSize.y * 0.5f);
    draw->AddText(textPos, IM_COL32(0, 0, 0, 255), digitStr.c_str());

    // Draw border if selected (darker green outline)
    if (showPositionDigit == digit) {
      constexpr ImU32 darkGreen = IM_COL32(0, 180, 0, 255);
      draw->AddCircle(buttonCenter, circleRadius, darkGreen, 16, 4.0f);
      draw->AddCircle(buttonCenter, circleRadius + 2.5f, darkGreen, 16, 1.5f);
    }

    ImGui::SameLine(100);

    // --- SHOW EFFECT BUTTON (digit with overstrike slash) ---
    // Check if all 9 of this digit are placed (disable button if so)
    int digitCount = 0;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        if (grid[r][c].value == digit) {
          digitCount++;
        }
      }
    }
    const bool allDigitsPlaced = (digitCount == 9);

    const ImVec2 effectButtonPos = ImGui::GetCursorScreenPos();
    const ImVec2 effectButtonCenter(effectButtonPos.x + digitButtonWidth * 0.5f,
                                    effectButtonPos.y + digitButtonHeight * 0.5f);

    // Invisible button for click detection
    ImGui::InvisibleButton(("eff" + std::to_string(digit)).c_str(), ImVec2(digitButtonWidth, digitButtonHeight));
    if (ImGui::IsItemClicked() && !allDigitsPlaced) {
      showEffectDigit = (showEffectDigit == digit) ? 0 : digit;
    }

    // Draw digit with overstrike slash (grayed out if all digits placed)
    const ImU32 effectColor = allDigitsPlaced ? IM_COL32(150, 150, 150, 255) : slashBlue;

    // Draw digit centered
    ImFont* effectFont = ImGui::GetFont();
    const ImVec2 effectTextSize = effectFont->CalcTextSizeA(effectFont->FontSize, FLT_MAX, 0, digitStr.c_str());
    const ImVec2 effectTextPos(
        effectButtonPos.x + (digitButtonWidth - effectTextSize.x) * 0.5f,
        effectButtonPos.y + (digitButtonHeight - effectTextSize.y) * 0.5f
    );
    draw->AddText(effectTextPos, effectColor, digitStr.c_str());

    // Draw overstrike slash through the digit
    draw->AddLine(
        ImVec2(effectButtonPos.x + 6, effectButtonPos.y + digitButtonHeight - 6),
        ImVec2(effectButtonPos.x + digitButtonWidth - 6, effectButtonPos.y + 6),
        effectColor, 2.5f
    );

    // Draw border if selected
    if (showEffectDigit == digit) {
      draw->AddRect(effectButtonPos, ImVec2(effectButtonPos.x + digitButtonWidth, effectButtonPos.y + digitButtonHeight),
                    slashBlue, 0.0f, 0, 2.0f);
    }

    ImGui::PopID();
  }

  ImGui::Separator();

  // --- PAIRS TOGGLE (two circles at 45 degree angle) ---
  const ImVec2 pairsButtonPos = ImGui::GetCursorScreenPos();
  const ImVec2 pairsButtonSize(compactTagWidth, compactTagHeight);
  const ImVec2 pairsButtonCenter(pairsButtonPos.x + pairsButtonSize.x * 0.5f,
                                 pairsButtonPos.y + pairsButtonSize.y * 0.5f);

  ImGui::InvisibleButton("pairs", pairsButtonSize);
  if (ImGui::IsItemClicked()) {
    highlightPairs = !highlightPairs;
  }

  // Draw two circles touching at 45 degree angle
  const float smallRadius = 6.0f;
  const float offset = smallRadius * 0.7f;  // Spacing for touching circles
  const ImVec2 circle1(pairsButtonCenter.x - offset, pairsButtonCenter.y - offset);
  const ImVec2 circle2(pairsButtonCenter.x + offset, pairsButtonCenter.y + offset);

  draw->AddCircleFilled(circle1, smallRadius, brightGreen);
  draw->AddCircleFilled(circle2, smallRadius, brightGreen);

  // Draw border if pairs are highlighted
  if (highlightPairs) {
    draw->AddCircle(circle1, smallRadius, brightGreen, 16, 2.0f);
    draw->AddCircle(circle2, smallRadius, brightGreen, 16, 2.0f);
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
