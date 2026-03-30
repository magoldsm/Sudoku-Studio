#include <algorithm>
#include <array>
#include <bitset>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include "hints_solver.h"
#include "imgui.h"
#include "snapshot_io.h"
#include "sudoku_core.h"
#include "ui_helpers.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace sudoku {

constexpr size_t kMaxUndoHistory = 256;

constexpr std::array<ImU32, 10> kTagColors = {
    0U,
    IM_COL32(254, 205, 211, 255),
    IM_COL32(254, 240, 138, 255),
    IM_COL32(220, 252, 231, 255),
    IM_COL32(224, 242, 254, 255),
    IM_COL32(233, 213, 255, 255),
    IM_COL32(254, 215, 170, 255),
    IM_COL32(199, 210, 254, 255),
    IM_COL32(191, 219, 254, 255),
    IM_COL32(217, 249, 157, 255),
};

  void PushUndoState(std::vector<Grid>& undoHistory, const Grid& grid) {
    if (undoHistory.size() >= kMaxUndoHistory) {
      undoHistory.erase(undoHistory.begin());
    }
    undoHistory.push_back(grid);
  }

  bool UndoLastChange(Grid& grid, std::vector<Grid>& undoHistory) {
    if (undoHistory.empty()) {
      return false;
    }
    grid = undoHistory.back();
    undoHistory.pop_back();
    return true;
  }

bool IsSafeInPuzzle(const Puzzle& board, int row, int col, int digit) {
  for (int i = 0; i < kGridSize; ++i) {
    if (board[row][i] == digit || board[i][col] == digit) {
      return false;
    }
  }

  const int boxRow = (row / 3) * 3;
  const int boxCol = (col / 3) * 3;
  for (int r = boxRow; r < boxRow + 3; ++r) {
    for (int c = boxCol; c < boxCol + 3; ++c) {
      if (board[r][c] == digit) {
        return false;
      }
    }
  }
  return true;
}

bool FillSolvedBoard(Puzzle& board, int index, std::mt19937& rng) {
  if (index >= 81) {
    return true;
  }

  const int row = index / 9;
  const int col = index % 9;
  if (board[row][col] != 0) {
    return FillSolvedBoard(board, index + 1, rng);
  }

  std::array<int, 9> digits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::shuffle(digits.begin(), digits.end(), rng);

  for (int digit : digits) {
    if (!IsSafeInPuzzle(board, row, col, digit)) {
      continue;
    }
    board[row][col] = digit;
    if (FillSolvedBoard(board, index + 1, rng)) {
      return true;
    }
    board[row][col] = 0;
  }

  return false;
}

bool SolveFirstSolution(Puzzle& board) {
  int bestRow = -1;
  int bestCol = -1;
  int bestCount = 10;
  std::array<int, 9> bestDigits{};

  for (int row = 0; row < 9; ++row) {
    for (int col = 0; col < 9; ++col) {
      if (board[row][col] != 0) {
        continue;
      }

      std::array<int, 9> digits{};
      int count = 0;
      for (int d = 1; d <= 9; ++d) {
        if (IsSafeInPuzzle(board, row, col, d)) {
          digits[count++] = d;
        }
      }

      if (count == 0) {
        return false;
      }

      if (count < bestCount) {
        bestCount = count;
        bestRow = row;
        bestCol = col;
        bestDigits = digits;
      }
    }
  }

  if (bestRow == -1) {
    return true;
  }

  for (int i = 0; i < bestCount; ++i) {
    board[bestRow][bestCol] = bestDigits[i];
    if (SolveFirstSolution(board)) {
      return true;
    }
  }

  board[bestRow][bestCol] = 0;
  return false;
}

int CountSolutions(Puzzle& board, int limit) {
  int bestRow = -1;
  int bestCol = -1;
  int bestCount = 10;
  std::array<int, 9> bestDigits{};

  for (int row = 0; row < 9; ++row) {
    for (int col = 0; col < 9; ++col) {
      if (board[row][col] != 0) {
        continue;
      }

      std::array<int, 9> digits{};
      int count = 0;
      for (int d = 1; d <= 9; ++d) {
        if (IsSafeInPuzzle(board, row, col, d)) {
          digits[count++] = d;
        }
      }

      if (count == 0) {
        return 0;
      }

      if (count < bestCount) {
        bestCount = count;
        bestRow = row;
        bestCol = col;
        bestDigits = digits;
      }
    }
  }

  if (bestRow == -1) {
    return 1;
  }

  int total = 0;
  for (int i = 0; i < bestCount; ++i) {
    board[bestRow][bestCol] = bestDigits[i];
    total += CountSolutions(board, limit - total);
    if (total >= limit) {
      board[bestRow][bestCol] = 0;
      return total;
    }
  }
  board[bestRow][bestCol] = 0;
  return total;
}

bool PlaceSingleIfUnique(Puzzle& board,
                         const std::array<std::array<std::bitset<9>, 9>, 9>& candidates,
                         int row,
                         int col,
                         int digit) {
  if (board[row][col] != 0 || !candidates[row][col].test(digit - 1)) {
    return false;
  }
  board[row][col] = digit;
  return true;
}

bool SolveWithSinglesOnly(Puzzle puzzle) {
  while (true) {
    bool changed = false;
    std::array<std::array<std::bitset<9>, 9>, 9> candidates{};

    for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
        if (puzzle[row][col] != 0) {
          continue;
        }
        for (int d = 1; d <= 9; ++d) {
          if (IsSafeInPuzzle(puzzle, row, col, d)) {
            candidates[row][col].set(d - 1);
          }
        }
        if (candidates[row][col].count() == 1) {
          for (int d = 1; d <= 9; ++d) {
            if (candidates[row][col].test(d - 1)) {
              puzzle[row][col] = d;
              changed = true;
              break;
            }
          }
        }
      }
    }

    for (int row = 0; row < 9; ++row) {
      for (int digit = 1; digit <= 9; ++digit) {
        int count = 0;
        int foundCol = -1;
        for (int col = 0; col < 9; ++col) {
          if (candidates[row][col].test(digit - 1)) {
            ++count;
            foundCol = col;
          }
        }
        if (count == 1) {
          changed = PlaceSingleIfUnique(puzzle, candidates, row, foundCol, digit) || changed;
        }
      }
    }

    for (int col = 0; col < 9; ++col) {
      for (int digit = 1; digit <= 9; ++digit) {
        int count = 0;
        int foundRow = -1;
        for (int row = 0; row < 9; ++row) {
          if (candidates[row][col].test(digit - 1)) {
            ++count;
            foundRow = row;
          }
        }
        if (count == 1) {
          changed = PlaceSingleIfUnique(puzzle, candidates, foundRow, col, digit) || changed;
        }
      }
    }

    for (int boxRow = 0; boxRow < 3; ++boxRow) {
      for (int boxCol = 0; boxCol < 3; ++boxCol) {
        for (int digit = 1; digit <= 9; ++digit) {
          int count = 0;
          int foundRow = -1;
          int foundCol = -1;
          for (int row = boxRow * 3; row < boxRow * 3 + 3; ++row) {
            for (int col = boxCol * 3; col < boxCol * 3 + 3; ++col) {
              if (candidates[row][col].test(digit - 1)) {
                ++count;
                foundRow = row;
                foundCol = col;
              }
            }
          }
          if (count == 1) {
            changed = PlaceSingleIfUnique(puzzle, candidates, foundRow, foundCol, digit) || changed;
          }
        }
      }
    }

    if (!changed) {
      break;
    }
  }

  for (int row = 0; row < 9; ++row) {
    for (int col = 0; col < 9; ++col) {
      if (puzzle[row][col] == 0) {
        return false;
      }
    }
  }
  return true;
}

Puzzle GenerateHardPuzzle(std::mt19937& rng) {
  constexpr int kMinClues = 24;
  constexpr int kMaxAttempts = 60;

  Puzzle best = {};
  int bestClues = 81;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Puzzle solved = {};
    FillSolvedBoard(solved, 0, rng);

    Puzzle puzzle = solved;
    std::vector<int> positions(81);
    for (int i = 0; i < 81; ++i) {
      positions[i] = i;
    }
    std::shuffle(positions.begin(), positions.end(), rng);

    int clues = 81;
    for (int idx : positions) {
      if (clues <= kMinClues) {
        break;
      }

      const int row = idx / 9;
      const int col = idx % 9;
      const int backup = puzzle[row][col];
      puzzle[row][col] = 0;

      Puzzle check = puzzle;
      if (CountSolutions(check, 2) != 1) {
        puzzle[row][col] = backup;
        continue;
      }
      --clues;
    }

    if (!SolveWithSinglesOnly(puzzle)) {
      return puzzle;
    }

    if (clues < bestClues) {
      best = puzzle;
      bestClues = clues;
    }
  }

  return best;
}

Puzzle GeneratePuzzleWithDifficulty(std::mt19937& rng, Difficulty difficulty) {
  constexpr int kMaxAttempts = 60;

  int targetMinClues = 24;
  bool requireNotSinglesSolvable = true;

  switch (difficulty) {
    case Difficulty::kSimple:
      targetMinClues = 40;
      requireNotSinglesSolvable = false;
      break;
    case Difficulty::kEasy:
      targetMinClues = 35;
      requireNotSinglesSolvable = false;
      break;
    case Difficulty::kMild:
      targetMinClues = 32;
      requireNotSinglesSolvable = true;
      break;
    case Difficulty::kModerate:
      targetMinClues = 28;
      requireNotSinglesSolvable = true;
      break;
    case Difficulty::kHard:
      targetMinClues = 24;
      requireNotSinglesSolvable = true;
      break;
    case Difficulty::kVeryHard:
      targetMinClues = 22;
      requireNotSinglesSolvable = true;
      break;
    case Difficulty::kFiendish:
      targetMinClues = 20;
      requireNotSinglesSolvable = true;
      break;
    case Difficulty::kDiabolical:
      targetMinClues = 17;
      requireNotSinglesSolvable = true;
      break;
  }

  Puzzle best = {};
  int bestClues = 81;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Puzzle solved = {};
    FillSolvedBoard(solved, 0, rng);

    Puzzle puzzle = solved;
    std::vector<int> positions(81);
    for (int i = 0; i < 81; ++i) {
      positions[i] = i;
    }
    std::shuffle(positions.begin(), positions.end(), rng);

    int clues = 81;
    for (int idx : positions) {
      if (clues <= targetMinClues) {
        break;
      }

      const int row = idx / 9;
      const int col = idx % 9;
      const int backup = puzzle[row][col];
      puzzle[row][col] = 0;

      Puzzle check = puzzle;
      if (CountSolutions(check, 2) != 1) {
        puzzle[row][col] = backup;
        continue;
      }
      --clues;
    }

    const bool singlesSolvable = SolveWithSinglesOnly(puzzle);
    
    if (requireNotSinglesSolvable) {
      if (!singlesSolvable) {
        return puzzle;
      }
    } else {
      if (singlesSolvable) {
        return puzzle;
      }
    }

    if (clues < bestClues) {
      best = puzzle;
      bestClues = clues;
    }
  }

  return best;
}

Grid BuildGrid(const Puzzle& puzzle) {
  Grid grid{};
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      grid[r][c].value = puzzle[r][c];
      grid[r][c].fixed = (puzzle[r][c] != 0);
    }
  }
  return grid;
}

bool ComputeSolutionFromGivens(const Puzzle& givens, Puzzle& solution) {
  Puzzle puzzle = givens;
  if (!SolveFirstSolution(puzzle)) {
    return false;
  }
  solution = puzzle;
  return true;
}

bool IsConflict(const Grid& grid, int row, int col, int value) {
  if (value == 0) {
    return false;
  }

  for (int c = 0; c < kGridSize; ++c) {
    if (c != col && grid[row][c].value == value) {
      return true;
    }
  }
  for (int r = 0; r < kGridSize; ++r) {
    if (r != row && grid[r][col].value == value) {
      return true;
    }
  }

  const int boxR = (row / 3) * 3;
  const int boxC = (col / 3) * 3;
  for (int r = boxR; r < boxR + 3; ++r) {
    for (int c = boxC; c < boxC + 3; ++c) {
      if ((r != row || c != col) && grid[r][c].value == value) {
        return true;
      }
    }
  }
  return false;
}

void RemoveDigitFromPeerPencils(Grid& grid, int row, int col, int digit) {
  if (digit < 1 || digit > 9) {
    return;
  }

  const int bit = digit - 1;

  for (int c = 0; c < kGridSize; ++c) {
    if (c == col || grid[row][c].value != 0) {
      continue;
    }
    grid[row][c].pencil.reset(bit);
  }

  for (int r = 0; r < kGridSize; ++r) {
    if (r == row || grid[r][col].value != 0) {
      continue;
    }
    grid[r][col].pencil.reset(bit);
  }

  const int boxR = (row / 3) * 3;
  const int boxC = (col / 3) * 3;
  for (int r = boxR; r < boxR + 3; ++r) {
    for (int c = boxC; c < boxC + 3; ++c) {
      if ((r == row && c == col) || grid[r][c].value != 0) {
        continue;
      }
      grid[r][c].pencil.reset(bit);
    }
  }
}

bool IsSolved(const Grid& grid) {
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      const int value = grid[r][c].value;
      if (value == 0 || IsConflict(grid, r, c, value)) {
        return false;
      }
    }
  }
  return true;
}

std::bitset<9> ComputeCandidates(const Grid& grid, int row, int col) {
  std::bitset<9> candidates;
  if (grid[row][col].value != 0) {
    return candidates;
  }

  for (int digit = 1; digit <= 9; ++digit) {
    if (!IsConflict(grid, row, col, digit)) {
      candidates.set(digit - 1);
    }
  }
  return candidates;
}

using CandidateGrid = std::array<std::array<std::bitset<9>, kGridSize>, kGridSize>;

struct UnitCells {
  std::array<HintCell, 9> cells;
};

CandidateGrid BuildCandidateGrid(const Grid& grid) {
  CandidateGrid candidates{};
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const std::bitset<9> legalCandidates = ComputeCandidates(grid, row, col);
      if (grid[row][col].value != 0) {
        candidates[row][col].reset();
        continue;
      }

      // Use visible pencil marks as the current candidate state when present,
      // but clamp them to legal values so stale notes do not survive placements.
      if (grid[row][col].pencil.any()) {
        candidates[row][col] = legalCandidates & grid[row][col].pencil;
      } else {
        candidates[row][col] = legalCandidates;
      }
    }
  }
  return candidates;
}

std::vector<UnitCells> BuildAllUnits() {
  std::vector<UnitCells> units;
  units.reserve(27);

  for (int row = 0; row < kGridSize; ++row) {
    UnitCells unit{};
    for (int col = 0; col < kGridSize; ++col) {
      unit.cells[col] = {row, col};
    }
    units.push_back(unit);
  }

  for (int col = 0; col < kGridSize; ++col) {
    UnitCells unit{};
    for (int row = 0; row < kGridSize; ++row) {
      unit.cells[row] = {row, col};
    }
    units.push_back(unit);
  }

  for (int boxRow = 0; boxRow < 3; ++boxRow) {
    for (int boxCol = 0; boxCol < 3; ++boxCol) {
      UnitCells unit{};
      int index = 0;
      for (int row = boxRow * 3; row < boxRow * 3 + 3; ++row) {
        for (int col = boxCol * 3; col < boxCol * 3 + 3; ++col) {
          unit.cells[index++] = {row, col};
        }
      }
      units.push_back(unit);
    }
  }

  return units;
}

bool ContainsCell(const std::vector<HintCell>& cells, int row, int col) {
  for (const HintCell& cell : cells) {
    if (cell.row == row && cell.col == col) {
      return true;
    }
  }
  return false;
}

void AddUniqueCell(std::vector<HintCell>& cells, int row, int col) {
  if (!ContainsCell(cells, row, col)) {
    cells.push_back({row, col});
  }
}

bool SeesBoth(const HintCell& target, const HintCell& left, const HintCell& right) {
  return SharesUnit(target.row, target.col, left.row, left.col) &&
         SharesUnit(target.row, target.col, right.row, right.col);
}

template <typename Func>
bool ForEachCombination(const std::vector<int>& values,
                        int choose,
                        int start,
                        std::vector<int>& current,
                        Func&& func) {
  if (static_cast<int>(current.size()) == choose) {
    return func(current);
  }

  for (int index = start; index <= static_cast<int>(values.size()) - (choose - static_cast<int>(current.size()));
       ++index) {
    current.push_back(values[index]);
    if (ForEachCombination(values, choose, index + 1, current, func)) {
      return true;
    }
    current.pop_back();
  }

  return false;
}

Hint MakeHint(const char* techniqueName, const std::vector<HintCell>& cells) {
  Hint hint;
  hint.techniqueName = techniqueName;
  hint.affectedCells = cells;
  return hint;
}

std::vector<int> ExtractDigits(const std::bitset<9>& mask) {
  std::vector<int> digits;
  for (int digit = 0; digit < 9; ++digit) {
    if (mask.test(digit)) {
      digits.push_back(digit + 1);
    }
  }
  return digits;
}

Hint MakeHint(const char* techniqueName,
              const std::vector<HintCell>& cells,
              const std::vector<int>& digits) {
  Hint hint;
  hint.techniqueName = techniqueName;
  hint.affectedCells = cells;
  hint.involvedDigits = digits;
  return hint;
}

Hint MakeHint(const char* techniqueName,
              const std::vector<HintCell>& cells,
              const std::vector<int>& digits,
              const std::vector<HintCell>& chainEndpoints) {
  Hint hint;
  hint.techniqueName = techniqueName;
  hint.affectedCells = cells;
  hint.involvedDigits = digits;
  hint.chainEndpoints = chainEndpoints;
  return hint;
}

Hint MakeHint(const char* techniqueName,
              const std::vector<HintCell>& cells,
              const std::vector<int>& digits,
              const std::vector<HintCell>& chainEndpoints,
              const std::vector<HintCell>& chainCells) {
  Hint hint;
  hint.techniqueName = techniqueName;
  hint.affectedCells = cells;
  hint.involvedDigits = digits;
  hint.chainEndpoints = chainEndpoints;
  hint.chainCells = chainCells;
  return hint;
}

bool SharesUnit(int r1, int c1, int r2, int c2) {
  if (r1 == r2 || c1 == c2) {
    return true;
  }
  return (r1 / 3 == r2 / 3) && (c1 / 3 == c2 / 3);
}

void ConfigureStyle() {
  ImGui::StyleColorsLight();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.CellPadding = ImVec2(8.0f, 8.0f);

  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.94f, 0.90f, 1.0f);
  style.Colors[ImGuiCol_TitleBg] = ImVec4(0.42f, 0.34f, 0.26f, 1.0f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.29f, 0.22f, 1.0f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.89f, 0.84f, 0.76f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.84f, 0.77f, 0.67f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.67f, 0.56f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.57f, 0.48f, 1.0f);
}

void DrawBoard(Grid& grid,
               int& selectedRow,
               int& selectedCol,
               int highlightDigit,
               InputMode mode,
               int& activeColor,
               bool highlightPairs,
               bool showWrongEntrySlash,
               const Puzzle* solution,
               bool hasSolution,
               std::vector<Grid>& undoHistory,
               bool solved,
               const Hint& hint,
               ImFont* digitFont,
               ImFont* noteFont) {
  ImGui::BeginChild("BoardArea", ImVec2(kBoardSize + 12.0f, kBoardSize + 18.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 boardMax(origin.x + kBoardSize, origin.y + kBoardSize);
  ImGui::InvisibleButton("board_canvas", ImVec2(kBoardSize, kBoardSize));

  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const int clickedCol = static_cast<int>((mouse.x - origin.x) / kCellSize);
    const int clickedRow = static_cast<int>((mouse.y - origin.y) / kCellSize);
    selectedCol = clickedCol;
    selectedRow = clickedRow;
    selectedCol = std::clamp(selectedCol, 0, 8);
    selectedRow = std::clamp(selectedRow, 0, 8);

    Cell& clickedCell = grid[selectedRow][selectedCol];
    if (mode == InputMode::kPencil && !clickedCell.fixed && clickedCell.value == 0) {
      const float localX = mouse.x - (origin.x + static_cast<float>(clickedCol) * kCellSize);
      const float localY = mouse.y - (origin.y + static_cast<float>(clickedRow) * kCellSize);
      const int subCol = std::clamp(static_cast<int>((localX / kCellSize) * 3.0f), 0, 2);
      const int subRow = std::clamp(static_cast<int>((localY / kCellSize) * 3.0f), 0, 2);
      const int markDigit = subRow * 3 + subCol + 1;
      if (clickedCell.pencil.test(markDigit - 1)) {
        PushUndoState(undoHistory, grid);
        clickedCell.pencil.reset(markDigit - 1);
      }
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();

  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      const ImVec2 cellMin(origin.x + c * kCellSize, origin.y + r * kCellSize);
      const ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);
      const bool hintCellActive = hint.revealPhase >= 2 && ContainsCell(hint.affectedCells, r, c);

      ImU32 fill = IM_COL32(253, 251, 246, 255);
      if (SharesUnit(r, c, selectedRow, selectedCol)) {
        fill = IM_COL32(247, 236, 209, 255);
      }
      if (highlightDigit != 0 && grid[r][c].value == 0 &&
          grid[r][c].pencil.test(highlightDigit - 1)) {
        // Matching pencil marks are highlighted per-note below, not by tinting the whole cell.
      }
      if (grid[r][c].colorTag > 0) {
        fill = kTagColors[grid[r][c].colorTag];
      }
      const int chainIndex = (hint.revealPhase >= 2) ? hint.ChainIndex(r, c) : -1;
      if (chainIndex >= 0) {
        fill = (chainIndex % 2 == 0) ? IM_COL32(138, 210, 255, 220) : IM_COL32(166, 235, 196, 220);
      }
      if (hintCellActive) {
        fill = IM_COL32(135, 206, 250, 200);
      }
      if (chainIndex >= 0) {
        fill = (chainIndex % 2 == 0) ? IM_COL32(138, 210, 255, 220) : IM_COL32(166, 235, 196, 220);
      }
      if (r == selectedRow && c == selectedCol) {
        fill = IM_COL32(255, 228, 173, 255);
      }

      draw->AddRectFilled(cellMin, cellMax, fill);

      const int value = grid[r][c].value;
      if (hint.revealPhase >= 3 && hintCellActive && value != 0 && hint.IncludesDigit(value)) {
        draw->AddRect(ImVec2(cellMin.x + 4.0f, cellMin.y + 4.0f),
                      ImVec2(cellMax.x - 4.0f, cellMax.y - 4.0f),
                      IM_COL32(255, 140, 0, 255), 6.0f, 0, 3.0f);
      }
      if (hint.revealPhase >= 2 && hint.IsChainEnd(r, c)) {
        draw->AddRect(ImVec2(cellMin.x + 2.0f, cellMin.y + 2.0f),
                      ImVec2(cellMax.x - 2.0f, cellMax.y - 2.0f),
                      IM_COL32(30, 100, 210, 255), 6.0f, 0, 3.0f);
      }
      if (highlightDigit != 0 && value == highlightDigit && !highlightPairs) {
        const ImVec2 center(cellMin.x + kCellSize * 0.5f, cellMin.y + kCellSize * 0.5f);
        draw->AddCircleFilled(center, kCellSize * 0.34f, IM_COL32(0, 255, 0, 120));
      }
      if (value != 0) {
        ImU32 textColor = grid[r][c].fixed ? IM_COL32(42, 36, 29, 255) : IM_COL32(34, 92, 126, 255);
        const bool hasConflict = IsConflict(grid, r, c, value);
        const bool wrongBySolution =
            hasSolution && solution != nullptr && !grid[r][c].fixed && value != (*solution)[r][c];
        if (hasConflict) {
          textColor = IM_COL32(192, 62, 62, 255);
        }
        if (wrongBySolution) {
          textColor = IM_COL32(192, 62, 62, 255);
        }
        if (solved) {
          textColor = IM_COL32(44, 128, 80, 255);
        }

        const std::string text = std::to_string(value);
        const ImVec2 sz = digitFont->CalcTextSizeA(digitFont->FontSize, FLT_MAX, 0.0f, text.c_str());
        draw->AddText(digitFont,
                      digitFont->FontSize,
                      ImVec2(cellMin.x + (kCellSize - sz.x) * 0.5f, cellMin.y + (kCellSize - sz.y) * 0.5f),
                      textColor,
                      text.c_str());

        if (showWrongEntrySlash && !grid[r][c].fixed && (hasConflict || wrongBySolution)) {
          draw->AddLine(ImVec2(cellMin.x + 8.0f, cellMax.y - 8.0f),
                        ImVec2(cellMax.x - 8.0f, cellMin.y + 8.0f),
                        IM_COL32(220, 40, 40, 255), 4.0f);
        }
        continue;
      }

      for (int d = 1; d <= 9; ++d) {
        if (!grid[r][c].pencil.test(d - 1)) {
          continue;
        }
        const int slotR = (d - 1) / 3;
        const int slotC = (d - 1) % 3;
        const std::string text = std::to_string(d);
        ImU32 noteColor = (highlightDigit == d) ? IM_COL32(35, 95, 130, 255) : IM_COL32(104, 96, 84, 255);
        const ImVec2 notePos(cellMin.x + 10.0f + slotC * 23.0f, cellMin.y + 7.0f + slotR * 20.0f);

        const ImVec2 noteSize =
            noteFont->CalcTextSizeA(noteFont->FontSize, FLT_MAX, 0.0f, text.c_str());
        if (highlightDigit == d || (highlightPairs && grid[r][c].pencil.count() == 2)) {
          draw->AddRectFilled(ImVec2(notePos.x - 2.0f, notePos.y - 1.0f),
                              ImVec2(notePos.x + noteSize.x + 2.0f, notePos.y + noteSize.y + 1.0f),
                              IM_COL32(0, 255, 0, 255), 3.0f);
        }
        if (hint.revealPhase >= 3 && hintCellActive && hint.IncludesDigit(d)) {
          draw->AddRect(ImVec2(notePos.x - 3.0f, notePos.y - 2.0f),
                        ImVec2(notePos.x + noteSize.x + 3.0f, notePos.y + noteSize.y + 2.0f),
                        IM_COL32(255, 140, 0, 255), 3.0f, 0, 2.0f);
        }

        draw->AddText(noteFont,
                      noteFont->FontSize,
                      notePos,
                      noteColor,
                      text.c_str());
      }
    }
  }

  for (int i = 0; i <= kGridSize; ++i) {
    const float x = origin.x + i * kCellSize;
    const float y = origin.y + i * kCellSize;
    const float thickness = (i % 3 == 0) ? 3.2f : 1.0f;
    const ImU32 color = (i % 3 == 0) ? IM_COL32(52, 44, 33, 255) : IM_COL32(139, 126, 104, 255);

    draw->AddLine(ImVec2(x, origin.y), ImVec2(x, boardMax.y), color, thickness);
    draw->AddLine(ImVec2(origin.x, y), ImVec2(boardMax.x, y), color, thickness);
  }

  ImGui::EndChild();

  ImGui::SameLine(0.0f, 12.0f);
  ImGui::BeginChild("TagPanel", ImVec2(164.0f, kBoardSize + 18.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

  ImGui::TextUnformatted("Color Tags");
  ImGui::Spacing();
  for (int tag = 1; tag <= 9; ++tag) {
    ImGui::PushID(tag);
    if (mode == InputMode::kColor && activeColor == tag) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.72f, 1.0f));
    } else {
      const ImVec4 c = ImGui::ColorConvertU32ToFloat4(kTagColors[tag]);
      ImGui::PushStyleColor(ImGuiCol_Button, c);
    }

    if (ImGui::Button(std::to_string(tag).c_str(), ImVec2(108.0f, 30.0f))) {
      activeColor = tag;
    }
    ImGui::PopStyleColor();
    ImGui::PopID();
  }

  ImGui::EndChild();
}

}  // namespace sudoku

using namespace sudoku;

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--self-check") {
    return RunHintSelfChecks();
  }

  if (!glfwInit()) {
    return 1;
  }

  const char* glslVersion = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  GLFWwindow* window = glfwCreateWindow(1280, 940, "Sudoku Studio", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ConfigureStyle();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  std::vector<std::string> fontCandidates = {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
  };

  ImFont* uiFont = nullptr;
  ImFont* digitFont = nullptr;
  ImFont* noteFont = nullptr;
  for (const std::string& fontPath : fontCandidates) {
    if (uiFont == nullptr) {
      uiFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 22.0f);
    }
    if (digitFont == nullptr) {
      digitFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 40.0f);
    }
    if (noteFont == nullptr) {
      noteFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    }
  }

  if (uiFont == nullptr || digitFont == nullptr || noteFont == nullptr) {
    uiFont = io.Fonts->AddFontDefault();
    digitFont = uiFont;
    noteFont = uiFont;
  }

  std::random_device rd;
  std::mt19937 rng(rd());

  Puzzle puzzle = GeneratePuzzleWithDifficulty(rng, Difficulty::kHard);
  Puzzle givens = puzzle;
  Grid grid = BuildGrid(puzzle);
  Puzzle solution = {};
  bool hasSolution = ComputeSolutionFromGivens(givens, solution);

  int selectedRow = 0;
  int selectedCol = 0;
  int activeColor = 1;
  bool highlightPairs = false;
  bool showWrongEntrySlash = true;
  InputMode mode = InputMode::kDigit;
  Difficulty selectedDifficulty = Difficulty::kHard;
  std::vector<Grid> undoHistory;
  std::string snapshotText;
  bool openSnapshotPopup = false;
  static char loadInputBuf[16384] = {};
  bool openLoadPopup = false;
  std::string loadErrorMessage;
  
  Hint currentHint;
  int hintPhaseCounter = 0;

  std::string statusMessage =
      "Menu/toolbar enabled. Ctrl+Z undo, P auto-pencil, K naked singles, H hidden singles";
  int statusFrames = 480;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::PushFont(uiFont);

    bool requestNewPuzzle = false;
    bool requestAutoPencil = false;
    bool requestSolveNakedSingles = false;
    bool requestSolveHiddenSingles = false;
    bool requestHint = false;
    bool requestUndo = false;
    bool requestSnapshot = false;
    bool requestLoad = false;

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::BeginMenu("New Puzzle")) {
          if (ImGui::MenuItem("Simple", nullptr, selectedDifficulty == Difficulty::kSimple)) {
            selectedDifficulty = Difficulty::kSimple;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Easy", nullptr, selectedDifficulty == Difficulty::kEasy)) {
            selectedDifficulty = Difficulty::kEasy;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Mild", nullptr, selectedDifficulty == Difficulty::kMild)) {
            selectedDifficulty = Difficulty::kMild;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Moderate", nullptr, selectedDifficulty == Difficulty::kModerate)) {
            selectedDifficulty = Difficulty::kModerate;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Hard", nullptr, selectedDifficulty == Difficulty::kHard)) {
            selectedDifficulty = Difficulty::kHard;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Very Hard", nullptr, selectedDifficulty == Difficulty::kVeryHard)) {
            selectedDifficulty = Difficulty::kVeryHard;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Fiendish", nullptr, selectedDifficulty == Difficulty::kFiendish)) {
            selectedDifficulty = Difficulty::kFiendish;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Diabolical", nullptr, selectedDifficulty == Difficulty::kDiabolical)) {
            selectedDifficulty = Difficulty::kDiabolical;
            requestNewPuzzle = true;
          }
          ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoHistory.empty())) {
          requestUndo = true;
        }
        if (ImGui::MenuItem("Snapshot", nullptr)) {
          requestSnapshot = true;
        }
        if (ImGui::MenuItem("Load Snapshot", nullptr)) {
          requestLoad = true;
        }
        if (ImGui::MenuItem("Auto Pencil", "P")) {
          requestAutoPencil = true;
        }
        if (ImGui::MenuItem("Solve Naked Singles", "K")) {
          requestSolveNakedSingles = true;
        }
        if (ImGui::MenuItem("Solve Hidden Singles", "H")) {
          requestSolveHiddenSingles = true;
        }
        if (ImGui::MenuItem("Highlight Pencil Pairs", "B", highlightPairs)) {
          highlightPairs = !highlightPairs;
          statusMessage = highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
          statusFrames = 200;
        }
        if (ImGui::MenuItem("Wrong Entry Slash", nullptr, showWrongEntrySlash)) {
          showWrongEntrySlash = !showWrongEntrySlash;
          statusMessage = showWrongEntrySlash ? "Wrong-entry slash enabled" : "Wrong-entry slash disabled";
          statusFrames = 200;
        }
        if (ImGui::MenuItem("Hint", "?")) {
          requestHint = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Mode")) {
        if (ImGui::MenuItem("Digit", "Q", mode == InputMode::kDigit)) {
          mode = InputMode::kDigit;
        }
        if (ImGui::MenuItem("Pencil", "E", mode == InputMode::kPencil)) {
          mode = InputMode::kPencil;
        }
        if (ImGui::MenuItem("Color", "R", mode == InputMode::kColor)) {
          mode = InputMode::kColor;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImVec2(0, 24), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 96), ImGuiCond_Always);
    ImGui::Begin("Toolbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::Button("New", ImVec2(90, 34))) {
      requestNewPuzzle = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo", ImVec2(90, 34)) && !undoHistory.empty()) {
      requestUndo = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Snapshot", ImVec2(110, 34))) {
      requestSnapshot = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load", ImVec2(75, 34))) {
      requestLoad = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    int difficultyIdx = static_cast<int>(selectedDifficulty);
    const char* difficultyItems[] = {"Simple", "Easy", "Mild", "Moderate", "Hard", "Very Hard", "Fiendish", "Diabolical"};
    if (ImGui::Combo("##difficulty", &difficultyIdx, difficultyItems, 8)) {
      selectedDifficulty = static_cast<Difficulty>(difficultyIdx);
    }
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::Text("Mode: %s", ModeName(mode));

    if (ImGui::Button("Auto Pencil", ImVec2(140, 34))) {
      requestAutoPencil = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Naked", ImVec2(95, 34))) {
      requestSolveNakedSingles = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hidden", ImVec2(95, 34))) {
      requestSolveHiddenSingles = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(highlightPairs ? "Pairs On" : "Pairs Off", ImVec2(110, 34))) {
      highlightPairs = !highlightPairs;
      statusMessage = highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
      statusFrames = 200;
    }
    ImGui::SameLine();
    if (ImGui::Button(showWrongEntrySlash ? "Slash On" : "Slash Off", ImVec2(105, 34))) {
      showWrongEntrySlash = !showWrongEntrySlash;
      statusMessage = showWrongEntrySlash ? "Wrong-entry slash enabled" : "Wrong-entry slash disabled";
      statusFrames = 200;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hint (?)", ImVec2(95, 34))) {
      requestHint = true;
    }
    ImGui::SameLine(0.0f, 22.0f);
    ImGui::Text("Q/E/R switch mode");

    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
      mode = InputMode::kDigit;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      mode = InputMode::kPencil;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      mode = InputMode::kColor;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
      requestAutoPencil = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_K)) {
      requestSolveNakedSingles = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H)) {
      requestSolveHiddenSingles = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_B)) {
      highlightPairs = !highlightPairs;
      statusMessage = highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
      statusFrames = 200;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_N)) {
      requestNewPuzzle = true;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      requestUndo = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Slash)) {
      requestHint = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_D)) {
      selectedCol = (selectedCol + 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_A)) {
      selectedCol = (selectedCol + kGridSize - 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_S)) {
      selectedRow = (selectedRow + 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_W)) {
      selectedRow = (selectedRow + kGridSize - 1) % kGridSize;
    }

    if (requestNewPuzzle) {
      puzzle = GeneratePuzzleWithDifficulty(rng, selectedDifficulty);
      givens = puzzle;
      grid = BuildGrid(puzzle);
      hasSolution = ComputeSolutionFromGivens(givens, solution);
      undoHistory.clear();
      currentHint = {};
      selectedRow = 0;
      selectedCol = 0;
      statusMessage = std::string("Generated new ") + DifficultyName(selectedDifficulty) + " puzzle";
      statusFrames = 240;
    }
    if (requestSnapshot) {
      snapshotText = SerializeSnapshot(grid, selectedDifficulty, mode, selectedRow, selectedCol,
                                       highlightPairs, showWrongEntrySlash, currentHint);
      openSnapshotPopup = true;
      statusMessage = "Snapshot captured";
      statusFrames = 200;
    }
    if (requestUndo) {
      currentHint = {};
      if (UndoLastChange(grid, undoHistory)) {
        statusMessage = "Undid last change";
      } else {
        statusMessage = "Nothing to undo";
      }
      statusFrames = 200;
    }
    if (requestAutoPencil) {
      const Grid previous = grid;
      const int changed = ApplyAutoPencil(grid);
      currentHint = {};
      if (changed > 0) {
        PushUndoState(undoHistory, previous);
      }
      statusMessage = "Auto-pencil updated " + std::to_string(changed) + " cells";
      statusFrames = 240;
    }
    if (requestSolveNakedSingles) {
      const Grid previous = grid;
      const int placed = AutoSolveNakedSingles(grid);
      currentHint = {};
      if (placed > 0) {
        PushUndoState(undoHistory, previous);
      }
      if (placed > 0) {
        statusMessage = "Solved " + std::to_string(placed) + " naked singles";
      } else {
        statusMessage = "No naked singles found";
      }
      statusFrames = 300;
    }
    if (requestSolveHiddenSingles) {
      const Grid previous = grid;
      const int placed = AutoSolveHiddenSingles(grid);
      currentHint = {};
      if (placed > 0) {
        PushUndoState(undoHistory, previous);
      }
      if (placed > 0) {
        statusMessage = "Solved " + std::to_string(placed) + " hidden singles";
      } else {
        statusMessage = "No hidden singles found";
      }
      statusFrames = 300;
    }
    if (requestHint) {
      if (currentHint.revealPhase == 0 || currentHint.revealPhase >= 3) {
        currentHint = GenerateHint(grid);
        currentHint.revealPhase = 1;
        statusMessage = "Hint: " + currentHint.techniqueName;
        statusFrames = 300;
        hintPhaseCounter = 0;
      } else if (currentHint.revealPhase == 1) {
        currentHint.revealPhase = 2;
        statusMessage = "Hint: " + currentHint.techniqueName + " (cells highlighted)";
        statusFrames = 400;
      } else if (currentHint.revealPhase == 2) {
        currentHint.revealPhase = 3;
        statusMessage = "Hint: " + currentHint.techniqueName + " (digits highlighted)";
        statusFrames = 400;
      }
    }

    Cell& selected = grid[selectedRow][selectedCol];
    const bool editable = !selected.fixed;
    const int digit = ReadDigitPress();

    if (editable && mode == InputMode::kDigit) {
      if (digit != 0) {
        if (selected.value != digit || selected.pencil.any()) {
          PushUndoState(undoHistory, grid);
          selected.value = digit;
          selected.pencil.reset();
          RemoveDigitFromPeerPencils(grid, selectedRow, selectedCol, digit);
          currentHint = {};
        }
      }
      if (IsClearPressed()) {
        if (selected.value != 0) {
          PushUndoState(undoHistory, grid);
          selected.value = 0;
          currentHint = {};
        }
      }
    }
    if (editable && mode == InputMode::kPencil) {
      if (digit != 0 && selected.value == 0) {
        PushUndoState(undoHistory, grid);
        selected.pencil.flip(digit - 1);
        currentHint = {};
      }
      if (IsClearPressed()) {
        if (selected.pencil.any()) {
          PushUndoState(undoHistory, grid);
          selected.pencil.reset();
          currentHint = {};
        }
      }
    }
    if (mode == InputMode::kColor) {
      if (digit != 0) {
        if (selected.colorTag != digit) {
          PushUndoState(undoHistory, grid);
        }
        activeColor = digit;
        selected.colorTag = activeColor;
        currentHint = {};
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        if (selected.colorTag != activeColor) {
          PushUndoState(undoHistory, grid);
          selected.colorTag = activeColor;
          currentHint = {};
        }
      }
      if (IsClearPressed()) {
        if (selected.colorTag != 0) {
          PushUndoState(undoHistory, grid);
          selected.colorTag = 0;
          currentHint = {};
        }
      }
    }

    const bool solved = IsSolved(grid);
    const int highlightDigit = selected.value;

    ImGui::SetNextWindowPos(ImVec2(14, 128), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 28, io.DisplaySize.y - 140), ImGuiCond_Always);
    ImGui::Begin("Sudoku Workspace", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Sudoku Studio - %s", DifficultyName(selectedDifficulty));
    ImGui::TextColored(ImVec4(0.33f, 0.27f, 0.20f, 1.0f),
                       "Sharp text rendering + menu/toolbar workflow");

    DrawBoard(grid, selectedRow, selectedCol, highlightDigit, mode, activeColor, highlightPairs,
          showWrongEntrySlash,
        &solution,
        hasSolution,
        undoHistory,
          solved, currentHint, digitFont, noteFont);

    ImGui::Separator();
    ImGui::Text(
      "Controls: arrows/WASD move | Q/E/R modes | 1-9 input | 0/Backspace/Delete clear | Ctrl+Z undo | B pairs | ? hint");
    ImGui::Text("Selected: row %d col %d", selectedRow + 1, selectedCol + 1);
    DrawTechniquePanel(currentHint);

    if (solved) {
      ImGui::TextColored(ImVec4(0.18f, 0.53f, 0.31f, 1.0f), "Solved: grid is complete and valid.");
    } else if (currentHint.revealPhase > 0 && !currentHint.techniqueName.empty()) {
      if (currentHint.revealPhase == 1) {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s",
                           currentHint.techniqueName.c_str());
      } else if (currentHint.revealPhase == 2) {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s (cells highlighted)",
                           currentHint.techniqueName.c_str());
      } else {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s (digits highlighted)",
                           currentHint.techniqueName.c_str());
      }
    } else if (statusFrames > 0) {
      ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "%s", statusMessage.c_str());
    } else {
      ImGui::TextUnformatted("Hints active: unit, same digit, and matching pencil-mark highlight");
    }

    if (openSnapshotPopup) {
      ImGui::OpenPopup("Board Snapshot");
      openSnapshotPopup = false;
    }
    if (requestLoad) {
      loadErrorMessage.clear();
      ImGui::OpenPopup("Load Snapshot");
    }

    if (ImGui::BeginPopupModal("Board Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Copy this snapshot into chat with a description of the bug.");
      ImGui::Spacing();
      if (ImGui::Button("Copy to Clipboard", ImVec2(160.0f, 0.0f))) {
        ImGui::SetClipboardText(snapshotText.c_str());
      }
      ImGui::SameLine();
      if (ImGui::Button("Close", ImVec2(90.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::Spacing();
      ImGui::BeginChild("SnapshotText", ImVec2(720.0f, 420.0f), true,
                        ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::TextUnformatted(snapshotText.c_str());
      ImGui::EndChild();
      ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Load Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Paste a snapshot below, then click Load.");
      ImGui::Spacing();
      ImGui::InputTextMultiline("##loadbuf", loadInputBuf, sizeof(loadInputBuf),
                                ImVec2(720.0f, 380.0f));
      ImGui::Spacing();
      if (!loadErrorMessage.empty()) {
        ImGui::TextColored(ImVec4(0.8f, 0.15f, 0.15f, 1.0f), "%s", loadErrorMessage.c_str());
        ImGui::Spacing();
      }
      if (ImGui::Button("Paste from Clipboard", ImVec2(190.0f, 0.0f))) {
        const char* cb = ImGui::GetClipboardText();
        if (cb) {
          const size_t len = std::min(strlen(cb), sizeof(loadInputBuf) - 1);
          memcpy(loadInputBuf, cb, len);
          loadInputBuf[len] = '\0';
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Load", ImVec2(80.0f, 0.0f))) {
        const SnapshotLoadResult loaded = DeserializeSnapshot(loadInputBuf);
        if (!loaded.ok) {
          loadErrorMessage = loaded.errorMessage;
        } else {
          PushUndoState(undoHistory, grid);
          grid = loaded.grid;
          givens = loaded.givens;
          hasSolution = ComputeSolutionFromGivens(givens, solution);
          selectedDifficulty = loaded.difficulty;
          mode = loaded.mode;
          selectedRow = std::clamp(loaded.selectedRow, 0, 8);
          selectedCol = std::clamp(loaded.selectedCol, 0, 8);
          currentHint = loaded.hint;
          loadErrorMessage.clear();
          memset(loadInputBuf, 0, sizeof(loadInputBuf));
          statusMessage = "Snapshot loaded";
          statusFrames = 240;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f))) {
        loadErrorMessage.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::End();

    if (statusFrames > 0) {
      --statusFrames;
    }

    ImGui::PopFont();

    ImGui::Render();
    int displayW = 0;
    int displayH = 0;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.95f, 0.93f, 0.89f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
