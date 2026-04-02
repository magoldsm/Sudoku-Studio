#pragma once

#include <string>
#include <vector>
#include "sudoku_core.h"
#include "hints_solver.h"

namespace sudoku {

// Application puzzle and grid state.
struct PuzzleState {
  Puzzle puzzle;
  Puzzle givens;
  Grid grid;
  Puzzle solution;
  bool hasSolution = false;
  PuzzleScore score;
};

// User interaction and UI state.
struct UIState {
  int selectedRow = 0;
  int selectedCol = 0;
  int activeColor = 1;
  bool highlightPairs = false;
  bool showWrongEntrySlash = true;
  InputMode mode = InputMode::kDigit;
  Difficulty selectedDifficulty = Difficulty::kHard;

  // Digit highlighting and effects (0 = none, 1-9 = digit)
  int showPositionDigit = 0;  // Highlight all cells with this digit
  int showEffectDigit = 0;    // Show blue slashes for impossible cells of this digit

  // Undo/redo history
  std::vector<Grid> undoHistory;

  // Hint state
  Hint currentHint;
  int hintPhaseCounter = 0;

  // Status messages
  std::string statusMessage;
  int statusFrames = 0;

  // Snapshot dialogs
  std::string snapshotText;
  bool openSnapshotPopup = false;
  static constexpr size_t kLoadInputBufSize = 16384;
  char loadInputBuf[kLoadInputBufSize] = {};
  bool openLoadPopup = false;
  std::string loadErrorMessage;

  // Score details popup
  bool openScoreDetailsPopup = false;
};

}  // namespace sudoku
