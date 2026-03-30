#pragma once

#include "sudoku_core.h"

namespace sudoku {

// Returns digit 1..9 when a number key is pressed, or 0 for no digit input.
int ReadDigitPress();
bool IsClearPressed();
const char* ModeName(InputMode mode);
const char* DifficultyName(Difficulty difficulty);
// Renders ordered technique list and highlights the active hint technique.
void DrawTechniquePanel(const Hint& currentHint);

}  // namespace sudoku
