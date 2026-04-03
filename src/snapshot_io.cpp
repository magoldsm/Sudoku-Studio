#include "snapshot_io.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace sudoku {
// Extracted from main.cpp during modular refactor.
// Snapshot serialization and deserialization helpers.
char EncodeCellValue(int value) {
  return value == 0 ? '.' : static_cast<char>('0' + value);
}

std::string SerializeBoardRows(const Grid& grid, bool fixedOnly) {
  std::string serialized;
  serialized.reserve(9 * 10);

  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      const int value = (fixedOnly && !cell.fixed) ? 0 : cell.value;
      serialized.push_back(EncodeCellValue(value));
    }
    if (row + 1 < kGridSize) {
      serialized.push_back('\n');
    }
  }

  return serialized;
}

std::string SerializeSnapshot(const Grid& grid,
                              Difficulty difficulty,
                              InputMode mode,
                              int selectedRow,
                              int selectedCol,
                              bool highlightPairs,
                              bool showWrongEntrySlash,
                              const Hint& hint) {
  std::ostringstream snapshot;
  // Header line is the compatibility gate for future format revisions.
  snapshot << "format=sudoku-studio-v1\n";
  snapshot << "difficulty=" << DifficultyName(difficulty) << "\n";
  snapshot << "mode=" << ModeName(mode) << "\n";
  snapshot << "selected_row=" << (selectedRow + 1) << "\n";
  snapshot << "selected_col=" << (selectedCol + 1) << "\n";
  snapshot << "highlight_pairs=" << (highlightPairs ? 1 : 0) << "\n";
  snapshot << "wrong_entry_slash=" << (showWrongEntrySlash ? 1 : 0) << "\n";
  snapshot << "hint_name=" << (hint.techniqueName.empty() ? "none" : hint.techniqueName) << "\n";
  snapshot << "hint_phase=" << hint.revealPhase << "\n";
  snapshot << "givens:\n" << SerializeBoardRows(grid, true) << "\n";
  snapshot << "values:\n" << SerializeBoardRows(grid, false) << "\n";
  snapshot << "pencils:\n";

  bool wrotePencil = false;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      if (cell.value != 0 || !cell.pencil.any()) {
        continue;
      }

      snapshot << "r" << (row + 1) << "c" << (col + 1) << "=";
      for (int digit = 1; digit <= 9; ++digit) {
        if (cell.pencil.test(digit - 1)) {
          snapshot << digit;
        }
      }
      snapshot << "\n";
      wrotePencil = true;
    }
  }
  if (!wrotePencil) {
    snapshot << "none\n";
  }

  snapshot << "colors:\n";
  bool wroteColor = false;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      if (cell.colorTag == 0) {
        continue;
      }

      snapshot << "r" << (row + 1) << "c" << (col + 1) << "=" << cell.colorTag << "\n";
      wroteColor = true;
    }
  }
  if (!wroteColor) {
    snapshot << "none\n";
  }

  snapshot << "hint_cells=";
  if (hint.affectedCells.empty()) {
    snapshot << "none";
  } else {
    for (size_t index = 0; index < hint.affectedCells.size(); ++index) {
      if (index > 0) {
        snapshot << ',';
      }
      snapshot << 'r' << (hint.affectedCells[index].row + 1)
               << 'c' << (hint.affectedCells[index].col + 1);
    }
  }
  snapshot << "\n";

  snapshot << "hint_digits=";
  if (hint.involvedDigits.empty()) {
    snapshot << "none";
  } else {
    for (size_t index = 0; index < hint.involvedDigits.size(); ++index) {
      if (index > 0) {
        snapshot << ',';
      }
      snapshot << hint.involvedDigits[index];
    }
  }
  snapshot << "\n";

  snapshot << "hint_chain_ends=";
  if (hint.chainEndpoints.empty()) {
    snapshot << "none";
  } else {
    for (size_t index = 0; index < hint.chainEndpoints.size(); ++index) {
      if (index > 0) {
        snapshot << ',';
      }
      snapshot << 'r' << (hint.chainEndpoints[index].row + 1)
               << 'c' << (hint.chainEndpoints[index].col + 1);
    }
  }
  snapshot << "\n";

  snapshot << "hint_chain_cells=";
  if (hint.chainCells.empty()) {
    snapshot << "none";
  } else {
    for (size_t index = 0; index < hint.chainCells.size(); ++index) {
      if (index > 0) {
        snapshot << ',';
      }
      snapshot << 'r' << (hint.chainCells[index].row + 1)
               << 'c' << (hint.chainCells[index].col + 1);
    }
  }
  snapshot << "\n";

  snapshot << "conflicts:\n";
  bool wroteConflict = false;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      if (cell.value == 0 || !IsConflict(grid, row, col, cell.value)) {
        continue;
      }
      snapshot << "r" << (row + 1) << "c" << (col + 1) << '=' << cell.value;
      if (cell.fixed) {
        snapshot << " fixed";
      }
      snapshot << "\n";
      wroteConflict = true;
    }
  }
  if (!wroteConflict) {
    snapshot << "none\n";
  }

  return snapshot.str();
}

SnapshotLoadResult DeserializeSnapshot(const std::string& text) {
  SnapshotLoadResult result;

  std::istringstream stream(text);
  std::string line;

  // Verify header before parsing any body content.
  if (!std::getline(stream, line)) {
    result.errorMessage = "Missing or wrong format header. Expected 'format=sudoku-studio-v1' on first line.";
    return result;
  }
  // Strip trailing whitespace (including \r from Windows line endings)
  while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
    line.pop_back();
  }
  if (line != "format=sudoku-studio-v1") {
    result.errorMessage = "Missing or wrong format header. Expected 'format=sudoku-studio-v1' on first line.";
    return result;
  }

  try {
    Grid grid{};
    Hint hint;

  enum class Section { kNone, kGivens, kValues, kPencils, kColors, kHintCells, kConflicts };
  Section section = Section::kNone;
  int boardRow = 0;

  // Track row completion so partial/malformed snapshots fail fast.
  Puzzle givensPuzzle{};
  Puzzle valuesPuzzle{};
  bool givensRead = false;
  bool valuesRead = false;

  while (std::getline(stream, line)) {
    // Strip trailing whitespace
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    // Section headers
    if (line == "givens:") { section = Section::kGivens; boardRow = 0; continue; }
    if (line == "values:") { section = Section::kValues; boardRow = 0; continue; }
    if (line == "pencils:") { section = Section::kPencils; continue; }
    if (line == "colors:") { section = Section::kColors; continue; }
    if (line == "conflicts:") { section = Section::kConflicts; continue; }

    // Key=value lines
    const auto eqPos = line.find('=');
    if (section == Section::kNone || section == Section::kPencils ||
        section == Section::kColors || section == Section::kConflicts) {
      if (eqPos == std::string::npos) {
        continue;
      }
      const std::string key = line.substr(0, eqPos);
      const std::string value = line.substr(eqPos + 1);

      if (key == "difficulty") {
        result.difficulty = ParseDifficulty(value.c_str());
      } else if (key == "mode") {
        result.mode = ParseMode(value.c_str());
      } else if (key == "selected_row") {
        result.selectedRow = std::max(0, std::stoi(value) - 1);
      } else if (key == "selected_col") {
        result.selectedCol = std::max(0, std::stoi(value) - 1);
      } else if (key == "highlight_pairs") {
        result.highlightPairs = (std::stoi(value) != 0);
      } else if (key == "wrong_entry_slash") {
        result.showWrongEntrySlash = (std::stoi(value) != 0);
      } else if (key == "hint_name") {
        hint.techniqueName = (value == "none") ? "" : value;
      } else if (key == "hint_phase") {
        hint.revealPhase = std::stoi(value);
      } else if (key == "hint_cells" && value != "none") {
        std::istringstream cs(value);
        std::string token;
        while (std::getline(cs, token, ',')) {
          if (token.size() >= 4 && token[0] == 'r') {
            const auto cPos = token.find('c', 1);
            if (cPos != std::string::npos) {
              const int r = std::stoi(token.substr(1, cPos - 1)) - 1;
              const int c = std::stoi(token.substr(cPos + 1)) - 1;
              if (r >= 0 && r < 9 && c >= 0 && c < 9) {
                hint.affectedCells.push_back({r, c});
              }
            }
          }
        }
      } else if (key == "hint_digits" && value != "none") {
        std::istringstream ds(value);
        std::string token;
        while (std::getline(ds, token, ',')) {
          hint.involvedDigits.push_back(std::stoi(token));
        }
      } else if (key == "hint_chain_ends" && value != "none") {
        std::istringstream cs(value);
        std::string token;
        while (std::getline(cs, token, ',')) {
          if (token.size() >= 4 && token[0] == 'r') {
            const auto cPos = token.find('c', 1);
            if (cPos != std::string::npos) {
              const int r = std::stoi(token.substr(1, cPos - 1)) - 1;
              const int c = std::stoi(token.substr(cPos + 1)) - 1;
              if (r >= 0 && r < 9 && c >= 0 && c < 9) {
                hint.chainEndpoints.push_back({r, c});
              }
            }
          }
        }
      } else if (key == "hint_chain_cells" && value != "none") {
        std::istringstream cs(value);
        std::string token;
        while (std::getline(cs, token, ',')) {
          if (token.size() >= 4 && token[0] == 'r') {
            const auto cPos = token.find('c', 1);
            if (cPos != std::string::npos) {
              const int r = std::stoi(token.substr(1, cPos - 1)) - 1;
              const int c = std::stoi(token.substr(cPos + 1)) - 1;
              if (r >= 0 && r < 9 && c >= 0 && c < 9) {
                hint.chainCells.push_back({r, c});
              }
            }
          }
        }
      } else if (section == Section::kPencils && value != "none") {
        // key is e.g. r2c3
        if (key.size() >= 4 && key[0] == 'r') {
          const auto cPos = key.find('c', 1);
          if (cPos != std::string::npos) {
            const int row = std::stoi(key.substr(1, cPos - 1)) - 1;
            const int col = std::stoi(key.substr(cPos + 1)) - 1;
            if (row >= 0 && row < 9 && col >= 0 && col < 9) {
              for (char ch : value) {
                if (ch >= '1' && ch <= '9') {
                  grid[row][col].pencil.set(ch - '1');
                }
              }
            }
          }
        }
      } else if (section == Section::kColors && value != "none") {
        if (key.size() >= 4 && key[0] == 'r') {
          const auto cPos = key.find('c', 1);
          if (cPos != std::string::npos) {
            const int row = std::stoi(key.substr(1, cPos - 1)) - 1;
            const int col = std::stoi(key.substr(cPos + 1)) - 1;
            if (row >= 0 && row < 9 && col >= 0 && col < 9) {
              grid[row][col].colorTag = std::stoi(value);
            }
          }
        }
      }
      continue;
    }

    // Multi-row board sections
    if (section == Section::kGivens && boardRow < 9) {
      if (static_cast<int>(line.size()) >= 9) {
        for (int col = 0; col < 9; ++col) {
          givensPuzzle[boardRow][col] = (line[col] == '.') ? 0 : (line[col] - '0');
        }
        ++boardRow;
        if (boardRow == 9) givensRead = true;
      }
      continue;
    }
    if (section == Section::kValues && boardRow < 9) {
      if (static_cast<int>(line.size()) >= 9) {
        for (int col = 0; col < 9; ++col) {
          valuesPuzzle[boardRow][col] = (line[col] == '.') ? 0 : (line[col] - '0');
        }
        ++boardRow;
        if (boardRow == 9) valuesRead = true;
      }
      continue;
    }
  }

  if (!givensRead || !valuesRead) {
    result.errorMessage = "Failed to read 9 rows from givens: or values: sections.";
    return result;
  }

  // Build the grid: fixed cells from givens, user cells from values.
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      grid[r][c].value = valuesPuzzle[r][c];
      grid[r][c].fixed = (givensPuzzle[r][c] != 0);
    }
  }

    result.grid = grid;
    result.givens = givensPuzzle;
    result.hint = hint;
    result.ok = true;
  } catch (const std::exception& e) {
    result.ok = false;
    result.errorMessage = std::string("Parse error: ") + e.what();
  }
  return result;
}
}  // namespace sudoku
