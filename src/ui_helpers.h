#pragma once

#include "sudoku_core.h"
#include <imgui.h>

namespace sudoku {

// Returns digit 1..9 when a number key is pressed, or 0 for no digit input.
int ReadDigitPress();
bool IsClearPressed();
const char* ModeName(InputMode mode);
const char* DifficultyName(Difficulty difficulty);
// Renders ordered technique list and highlights the active hint technique.
void DrawTechniquePanel(const Hint& currentHint);

// Drawing helpers
void DrawColorTagPanel(int& activeColor, InputMode mode,
                       const std::array<ImU32, 10>& tagColors);

void DrawGridLines(ImDrawList* draw, const ImVec2& origin, const ImVec2& boardMax);

}  // namespace sudoku
