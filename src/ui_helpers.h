#pragma once

#include "sudoku_core.h"
#include <imgui.h>

namespace sudoku {

// Returns digit 1..9 when a number key is pressed, or 0 for no digit input.
int ReadDigitPress();
bool IsClearPressed();
// Renders ordered technique list and highlights the active hint technique.
// If width > 0, uses that width; otherwise uses full available width.
void DrawTechniquePanel(const Hint& currentHint, float width = 0.0f);

// Drawing helpers
void DrawColorTagPanel(Grid& grid,
                       int& activeColor, InputMode& mode,
                       const std::array<ImU32, 10>& tagColors,
                       int& showPositionDigit, int& showEffectDigit,
                       bool& highlightPairs);

void DrawGridLines(ImDrawList* draw, const ImVec2& origin, const ImVec2& boardMax);

}  // namespace sudoku
