#pragma once

#include "sudoku_core.h"

namespace sudoku {

struct SnapshotLoadResult {
  bool ok = false;
  std::string errorMessage;
  Grid grid;
  Puzzle givens{};
  Difficulty difficulty = Difficulty::kHard;
  InputMode mode = InputMode::kDigit;
  int selectedRow = 0;
  int selectedCol = 0;
  Hint hint;
  bool highlightPairs = false;
  bool showWrongEntrySlash = false;
};

// Serializes full board/session state in sudoku-studio-v1 text format.
std::string SerializeSnapshot(const Grid& grid,
                              Difficulty difficulty,
                              InputMode mode,
                              int selectedRow,
                              int selectedCol,
                              bool highlightPairs,
                              bool showWrongEntrySlash,
                              const Hint& hint);

// Parses sudoku-studio-v1 text and returns either loaded state or error details.
SnapshotLoadResult DeserializeSnapshot(const std::string& text);

}  // namespace sudoku
