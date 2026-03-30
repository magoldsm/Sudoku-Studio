#include <algorithm>
#include <array>
#include <bitset>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace {

constexpr int kGridSize = 9;
constexpr float kCellSize = 72.0f;
constexpr float kBoardSize = kCellSize * static_cast<float>(kGridSize);

using Puzzle = std::array<std::array<int, kGridSize>, kGridSize>;

enum class InputMode {
  kDigit,
  kPencil,
  kColor,
};

enum class Difficulty {
  kSimple,
  kEasy,
  kMild,
  kModerate,
  kHard,
  kVeryHard,
  kFiendish,
  kDiabolical,
};

struct Cell {
  int value = 0;
  std::bitset<9> pencil;
  int colorTag = 0;
  bool fixed = false;
};

using Grid = std::array<std::array<Cell, kGridSize>, kGridSize>;

constexpr size_t kMaxUndoHistory = 256;

struct HintCell {
  int row = -1;
  int col = -1;
};

struct Hint {
  std::string techniqueName;
  std::vector<HintCell> affectedCells;
  std::vector<int> involvedDigits;
  std::vector<HintCell> chainEndpoints;
  std::vector<HintCell> chainCells;
  int revealPhase = 0;  // 0 = none, 1 = name shown, 2 = cells highlighted, 3 = digits highlighted
  
  bool IsValid() const {
    return !techniqueName.empty() && !affectedCells.empty();
  }

  bool IncludesDigit(int digit) const {
    return std::find(involvedDigits.begin(), involvedDigits.end(), digit) != involvedDigits.end();
  }

  bool IsChainEnd(int row, int col) const {
    for (const HintCell& cell : chainEndpoints) {
      if (cell.row == row && cell.col == col) {
        return true;
      }
    }
    return false;
  }

  int ChainIndex(int row, int col) const {
    for (int index = 0; index < static_cast<int>(chainCells.size()); ++index) {
      if (chainCells[index].row == row && chainCells[index].col == col) {
        return index;
      }
    }
    return -1;
  }
};

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

  bool SharesUnit(int r1, int c1, int r2, int c2);
  int ApplyAutoPencil(Grid& grid);
  int ApplyNakedSingles(Grid& grid);
  int ApplyHiddenSingles(Grid& grid);

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

Hint DetectNakedSingles(const CandidateGrid& candidates) {
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      if (candidates[row][col].count() == 1) {
        return MakeHint("Naked Single", {{row, col}}, ExtractDigits(candidates[row][col]));
      }
    }
  }
  return {};
}

Hint DetectHiddenSingles(const CandidateGrid& candidates) {
  const std::vector<UnitCells> units = BuildAllUnits();
  for (const UnitCells& unit : units) {
    for (int digit = 1; digit <= 9; ++digit) {
      int count = 0;
      HintCell found;
      for (const HintCell& cell : unit.cells) {
        if (candidates[cell.row][cell.col].test(digit - 1)) {
          ++count;
          found = cell;
        }
      }
      if (count == 1) {
        return MakeHint("Hidden Single", {found}, {digit});
      }
    }
  }
  return {};
}

Hint DetectPointingPairs(const CandidateGrid& candidates) {
  for (int boxRow = 0; boxRow < 3; ++boxRow) {
    for (int boxCol = 0; boxCol < 3; ++boxCol) {
      const int startRow = boxRow * 3;
      const int startCol = boxCol * 3;
      for (int digit = 1; digit <= 9; ++digit) {
        std::vector<HintCell> boxCells;
        for (int row = startRow; row < startRow + 3; ++row) {
          for (int col = startCol; col < startCol + 3; ++col) {
            if (candidates[row][col].test(digit - 1)) {
              boxCells.push_back({row, col});
            }
          }
        }
        if (boxCells.size() < 2 || boxCells.size() > 3) {
          continue;
        }

        bool sameRow = true;
        bool sameCol = true;
        for (const HintCell& cell : boxCells) {
          sameRow = sameRow && (cell.row == boxCells.front().row);
          sameCol = sameCol && (cell.col == boxCells.front().col);
        }

        std::vector<HintCell> affected = boxCells;
        bool hasElimination = false;
        if (sameRow) {
          const int row = boxCells.front().row;
          for (int col = 0; col < kGridSize; ++col) {
            if (col >= startCol && col < startCol + 3) {
              continue;
            }
            if (candidates[row][col].test(digit - 1)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }
        if (sameCol) {
          const int col = boxCells.front().col;
          for (int row = 0; row < kGridSize; ++row) {
            if (row >= startRow && row < startRow + 3) {
              continue;
            }
            if (candidates[row][col].test(digit - 1)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }

        if (hasElimination) {
          return MakeHint("Pointing Pair/Triple", affected, {digit});
        }
      }
    }
  }
  return {};
}

Hint DetectBoxLineReduction(const CandidateGrid& candidates) {
  for (int digit = 1; digit <= 9; ++digit) {
    for (int row = 0; row < kGridSize; ++row) {
      std::vector<HintCell> rowCells;
      for (int col = 0; col < kGridSize; ++col) {
        if (candidates[row][col].test(digit - 1)) {
          rowCells.push_back({row, col});
        }
      }
      if (rowCells.size() < 2) {
        continue;
      }
      int boxCol = rowCells.front().col / 3;
      bool sameBox = true;
      for (const HintCell& cell : rowCells) {
        sameBox = sameBox && (cell.col / 3 == boxCol);
      }
      if (sameBox) {
        std::vector<HintCell> affected = rowCells;
        const int startRow = (row / 3) * 3;
        const int startCol = boxCol * 3;
        bool hasElimination = false;
        for (int r = startRow; r < startRow + 3; ++r) {
          if (r == row) {
            continue;
          }
          for (int c = startCol; c < startCol + 3; ++c) {
            if (candidates[r][c].test(digit - 1)) {
              AddUniqueCell(affected, r, c);
              hasElimination = true;
            }
          }
        }
        if (hasElimination) {
          return MakeHint("Box/Line Reduction", affected, {digit});
        }
      }
    }

    for (int col = 0; col < kGridSize; ++col) {
      std::vector<HintCell> colCells;
      for (int row = 0; row < kGridSize; ++row) {
        if (candidates[row][col].test(digit - 1)) {
          colCells.push_back({row, col});
        }
      }
      if (colCells.size() < 2) {
        continue;
      }
      int boxRow = colCells.front().row / 3;
      bool sameBox = true;
      for (const HintCell& cell : colCells) {
        sameBox = sameBox && (cell.row / 3 == boxRow);
      }
      if (sameBox) {
        std::vector<HintCell> affected = colCells;
        const int startRow = boxRow * 3;
        const int startCol = (col / 3) * 3;
        bool hasElimination = false;
        for (int r = startRow; r < startRow + 3; ++r) {
          for (int c = startCol; c < startCol + 3; ++c) {
            if (c == col) {
              continue;
            }
            if (candidates[r][c].test(digit - 1)) {
              AddUniqueCell(affected, r, c);
              hasElimination = true;
            }
          }
        }
        if (hasElimination) {
          return MakeHint("Box/Line Reduction", affected, {digit});
        }
      }
    }
  }

  return {};
}

Hint DetectBlockBlockInteraction(const CandidateGrid& candidates) {
  // Same band: if a digit is confined to the same two rows in two boxes,
  // eliminate that digit from those rows in the third box.
  for (int band = 0; band < 3; ++band) {
    const int startRow = band * 3;
    for (int digit = 1; digit <= 9; ++digit) {
      std::array<std::bitset<3>, 3> rowMasks{};
      for (int boxCol = 0; boxCol < 3; ++boxCol) {
        const int startCol = boxCol * 3;
        std::bitset<3> rows;
        for (int r = 0; r < 3; ++r) {
          for (int c = 0; c < 3; ++c) {
            if (candidates[startRow + r][startCol + c].test(digit - 1)) {
              rows.set(r);
            }
          }
        }
        rowMasks[boxCol] = rows;
      }

      for (int boxA = 0; boxA < 2; ++boxA) {
        for (int boxB = boxA + 1; boxB < 3; ++boxB) {
          const std::bitset<3> shared = rowMasks[boxA] & rowMasks[boxB];
          if (shared.count() != 2 || rowMasks[boxA] != shared || rowMasks[boxB] != shared) {
            continue;
          }

          int boxC = 3 - boxA - boxB;
          std::vector<HintCell> affected;
          bool hasElimination = false;

          const int colStartA = boxA * 3;
          const int colStartB = boxB * 3;
          for (int rr = 0; rr < 3; ++rr) {
            if (!shared.test(rr)) {
              continue;
            }
            const int row = startRow + rr;
            for (int cc = 0; cc < 3; ++cc) {
              if (candidates[row][colStartA + cc].test(digit - 1)) {
                AddUniqueCell(affected, row, colStartA + cc);
              }
              if (candidates[row][colStartB + cc].test(digit - 1)) {
                AddUniqueCell(affected, row, colStartB + cc);
              }
            }
          }

          const int colStartC = boxC * 3;
          for (int rr = 0; rr < 3; ++rr) {
            if (!shared.test(rr)) {
              continue;
            }
            const int row = startRow + rr;
            for (int cc = 0; cc < 3; ++cc) {
              const int col = colStartC + cc;
              if (candidates[row][col].test(digit - 1)) {
                AddUniqueCell(affected, row, col);
                hasElimination = true;
              }
            }
          }

          if (hasElimination) {
            return MakeHint("Block/Block Interaction", affected, {digit});
          }
        }
      }
    }
  }

  // Same stack: if a digit is confined to the same two columns in two boxes,
  // eliminate that digit from those columns in the third box.
  for (int stack = 0; stack < 3; ++stack) {
    const int startCol = stack * 3;
    for (int digit = 1; digit <= 9; ++digit) {
      std::array<std::bitset<3>, 3> colMasks{};
      for (int boxRow = 0; boxRow < 3; ++boxRow) {
        const int startRow = boxRow * 3;
        std::bitset<3> cols;
        for (int r = 0; r < 3; ++r) {
          for (int c = 0; c < 3; ++c) {
            if (candidates[startRow + r][startCol + c].test(digit - 1)) {
              cols.set(c);
            }
          }
        }
        colMasks[boxRow] = cols;
      }

      for (int boxA = 0; boxA < 2; ++boxA) {
        for (int boxB = boxA + 1; boxB < 3; ++boxB) {
          const std::bitset<3> shared = colMasks[boxA] & colMasks[boxB];
          if (shared.count() != 2 || colMasks[boxA] != shared || colMasks[boxB] != shared) {
            continue;
          }

          int boxC = 3 - boxA - boxB;
          std::vector<HintCell> affected;
          bool hasElimination = false;

          const int rowStartA = boxA * 3;
          const int rowStartB = boxB * 3;
          for (int cc = 0; cc < 3; ++cc) {
            if (!shared.test(cc)) {
              continue;
            }
            const int col = startCol + cc;
            for (int rr = 0; rr < 3; ++rr) {
              if (candidates[rowStartA + rr][col].test(digit - 1)) {
                AddUniqueCell(affected, rowStartA + rr, col);
              }
              if (candidates[rowStartB + rr][col].test(digit - 1)) {
                AddUniqueCell(affected, rowStartB + rr, col);
              }
            }
          }

          const int rowStartC = boxC * 3;
          for (int cc = 0; cc < 3; ++cc) {
            if (!shared.test(cc)) {
              continue;
            }
            const int col = startCol + cc;
            for (int rr = 0; rr < 3; ++rr) {
              const int row = rowStartC + rr;
              if (candidates[row][col].test(digit - 1)) {
                AddUniqueCell(affected, row, col);
                hasElimination = true;
              }
            }
          }

          if (hasElimination) {
            return MakeHint("Block/Block Interaction", affected, {digit});
          }
        }
      }
    }
  }

  return {};
}

Hint DetectNakedSubset(const CandidateGrid& candidates, int subsetSize, const char* name) {
  const std::vector<UnitCells> units = BuildAllUnits();
  for (const UnitCells& unit : units) {
    std::vector<int> candidateIndices;
    for (int index = 0; index < 9; ++index) {
      const HintCell& cell = unit.cells[index];
      const size_t count = candidates[cell.row][cell.col].count();
      if (count >= 2 && count <= static_cast<size_t>(subsetSize)) {
        candidateIndices.push_back(index);
      }
    }
    if (static_cast<int>(candidateIndices.size()) < subsetSize) {
      continue;
    }

    std::vector<int> current;
    try {
      ForEachCombination(candidateIndices, subsetSize, 0, current, [&](const std::vector<int>& choice) {
        std::bitset<9> unionMask;
        std::vector<HintCell> affected;
        for (int index : choice) {
          const HintCell& cell = unit.cells[index];
          unionMask |= candidates[cell.row][cell.col];
          affected.push_back(cell);
        }
        if (static_cast<int>(unionMask.count()) != subsetSize) {
          return false;
        }

        bool hasElimination = false;
        for (int index = 0; index < 9; ++index) {
          if (std::find(choice.begin(), choice.end(), index) != choice.end()) {
            continue;
          }
          const HintCell& cell = unit.cells[index];
          if ((candidates[cell.row][cell.col] & unionMask).any()) {
            AddUniqueCell(affected, cell.row, cell.col);
            hasElimination = true;
          }
        }

        if (hasElimination) {
          throw MakeHint(name, affected, ExtractDigits(unionMask));
        }
        return false;
      });
    } catch (const Hint& hint) {
      return hint;
    }
  }
  return {};
}

Hint DetectHiddenSubset(const CandidateGrid& candidates, int subsetSize, const char* name) {
  const std::vector<UnitCells> units = BuildAllUnits();
  const std::vector<int> digits = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  for (const UnitCells& unit : units) {
    std::vector<int> current;
    try {
      ForEachCombination(digits, subsetSize, 0, current, [&](const std::vector<int>& choice) {
        std::vector<HintCell> affected;
        std::bitset<9> chosenMask;
        for (int digit : choice) {
          chosenMask.set(digit);
        }

        // Each chosen digit must appear in this unit at least twice and no more
        // than the subset size, otherwise this is either a hidden single or not
        // a proper hidden subset.
        for (int digit : choice) {
          int digitCount = 0;
          for (const HintCell& cell : unit.cells) {
            if (candidates[cell.row][cell.col].test(digit)) {
              ++digitCount;
            }
          }
          if (digitCount < 2 || digitCount > subsetSize) {
            return false;
          }
        }

        for (const HintCell& cell : unit.cells) {
          if ((candidates[cell.row][cell.col] & chosenMask).any()) {
            AddUniqueCell(affected, cell.row, cell.col);
          }
        }

        if (static_cast<int>(affected.size()) != subsetSize) {
          return false;
        }

        bool hasRestriction = false;
        bool alreadyNakedSubset = true;
        for (const HintCell& cell : affected) {
          if ((candidates[cell.row][cell.col] & ~chosenMask).any()) {
            hasRestriction = true;
          }
          if ((candidates[cell.row][cell.col] & chosenMask) != candidates[cell.row][cell.col]) {
            alreadyNakedSubset = false;
          }
        }

        // If the cells already contain only the chosen digits, this is either a
        // naked subset or a no-op, so do not mislabel it as hidden.
        if (hasRestriction && !alreadyNakedSubset) {
          throw MakeHint(name, affected, ExtractDigits(chosenMask));
        }
        return false;
      });
    } catch (const Hint& hint) {
      return hint;
    }
  }
  return {};
}

Hint DetectFish(const CandidateGrid& candidates, int size, const char* name) {
  const std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  for (int digit = 1; digit <= 9; ++digit) {
    std::vector<int> baseRows;
    for (int row = 0; row < kGridSize; ++row) {
      int count = 0;
      for (int col = 0; col < kGridSize; ++col) {
        if (candidates[row][col].test(digit - 1)) {
          ++count;
        }
      }
      if (count >= 2 && count <= size) {
        baseRows.push_back(row);
      }
    }

    std::vector<int> choice;
    try {
      ForEachCombination(baseRows, size, 0, choice, [&](const std::vector<int>& rows) {
        std::bitset<9> colsMask;
        std::vector<HintCell> affected;
        for (int row : rows) {
          for (int col = 0; col < kGridSize; ++col) {
            if (candidates[row][col].test(digit - 1)) {
              colsMask.set(col);
              AddUniqueCell(affected, row, col);
            }
          }
        }
        if (static_cast<int>(colsMask.count()) != size) {
          return false;
        }

        bool hasElimination = false;
        for (int row = 0; row < kGridSize; ++row) {
          if (std::find(rows.begin(), rows.end(), row) != rows.end()) {
            continue;
          }
          for (int col = 0; col < kGridSize; ++col) {
            if (colsMask.test(col) && candidates[row][col].test(digit - 1)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }

        if (hasElimination) {
          throw MakeHint(name, affected, {digit});
        }
        return false;
      });
    } catch (const Hint& hint) {
      return hint;
    }

    std::vector<int> baseCols;
    for (int col = 0; col < kGridSize; ++col) {
      int count = 0;
      for (int row = 0; row < kGridSize; ++row) {
        if (candidates[row][col].test(digit - 1)) {
          ++count;
        }
      }
      if (count >= 2 && count <= size) {
        baseCols.push_back(col);
      }
    }

    choice.clear();
    try {
      ForEachCombination(baseCols, size, 0, choice, [&](const std::vector<int>& cols) {
        std::bitset<9> rowsMask;
        std::vector<HintCell> affected;
        for (int col : cols) {
          for (int row = 0; row < kGridSize; ++row) {
            if (candidates[row][col].test(digit - 1)) {
              rowsMask.set(row);
              AddUniqueCell(affected, row, col);
            }
          }
        }
        if (static_cast<int>(rowsMask.count()) != size) {
          return false;
        }

        bool hasElimination = false;
        for (int col = 0; col < kGridSize; ++col) {
          if (std::find(cols.begin(), cols.end(), col) != cols.end()) {
            continue;
          }
          for (int row = 0; row < kGridSize; ++row) {
            if (rowsMask.test(row) && candidates[row][col].test(digit - 1)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }

        if (hasElimination) {
          throw MakeHint(name, affected, {digit});
        }
        return false;
      });
    } catch (const Hint& hint) {
      return hint;
    }
  }

  return {};
}

Hint DetectXYWing(const CandidateGrid& candidates) {
  std::vector<HintCell> bivalueCells;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      if (candidates[row][col].count() == 2) {
        bivalueCells.push_back({row, col});
      }
    }
  }

  for (const HintCell& pivot : bivalueCells) {
    const std::bitset<9> pivotMask = candidates[pivot.row][pivot.col];
    for (const HintCell& wingA : bivalueCells) {
      if ((wingA.row == pivot.row && wingA.col == pivot.col) ||
          !SharesUnit(pivot.row, pivot.col, wingA.row, wingA.col)) {
        continue;
      }
      const std::bitset<9> maskA = candidates[wingA.row][wingA.col];
      const std::bitset<9> sharedA = pivotMask & maskA;
      if (sharedA.count() != 1) {
        continue;
      }
      for (const HintCell& wingB : bivalueCells) {
        if ((wingB.row == pivot.row && wingB.col == pivot.col) ||
            (wingB.row == wingA.row && wingB.col == wingA.col) ||
            !SharesUnit(pivot.row, pivot.col, wingB.row, wingB.col)) {
          continue;
        }
        const std::bitset<9> maskB = candidates[wingB.row][wingB.col];
        const std::bitset<9> sharedB = pivotMask & maskB;
        if (sharedB.count() != 1 || sharedA == sharedB) {
          continue;
        }
        const std::bitset<9> zMask = maskA & maskB;
        if (zMask.count() != 1) {
          continue;
        }

        int zDigit = -1;
        for (int digit = 0; digit < 9; ++digit) {
          if (zMask.test(digit)) {
            zDigit = digit;
          }
        }

        std::vector<HintCell> affected = {pivot, wingA, wingB};
        bool hasElimination = false;
        for (int row = 0; row < kGridSize; ++row) {
          for (int col = 0; col < kGridSize; ++col) {
            if ((row == pivot.row && col == pivot.col) || (row == wingA.row && col == wingA.col) ||
                (row == wingB.row && col == wingB.col)) {
              continue;
            }
            if (candidates[row][col].test(zDigit) && SeesBoth({row, col}, wingA, wingB)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }
        if (hasElimination) {
          return MakeHint("Y-Wing", affected, ExtractDigits(pivotMask | maskA | maskB));
        }
      }
    }
  }

  return {};
}

Hint DetectXYZWing(const CandidateGrid& candidates) {
  std::vector<HintCell> bivalueCells;
  std::vector<HintCell> trivalueCells;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const size_t count = candidates[row][col].count();
      if (count == 2) {
        bivalueCells.push_back({row, col});
      } else if (count == 3) {
        trivalueCells.push_back({row, col});
      }
    }
  }

  for (const HintCell& pivot : trivalueCells) {
    const std::bitset<9> pivotMask = candidates[pivot.row][pivot.col];
    for (const HintCell& wingA : bivalueCells) {
      if (!SharesUnit(pivot.row, pivot.col, wingA.row, wingA.col)) {
        continue;
      }
      const std::bitset<9> maskA = candidates[wingA.row][wingA.col];
      if ((maskA & pivotMask) != maskA) {
        continue;
      }
      for (const HintCell& wingB : bivalueCells) {
        if ((wingB.row == wingA.row && wingB.col == wingA.col) ||
            !SharesUnit(pivot.row, pivot.col, wingB.row, wingB.col)) {
          continue;
        }
        const std::bitset<9> maskB = candidates[wingB.row][wingB.col];
        if ((maskB & pivotMask) != maskB) {
          continue;
        }
        const std::bitset<9> unionMask = maskA | maskB;
        if (unionMask != pivotMask || (maskA & maskB).count() != 1) {
          continue;
        }

        int zDigit = -1;
        for (int digit = 0; digit < 9; ++digit) {
          if ((maskA & maskB).test(digit)) {
            zDigit = digit;
          }
        }
        std::vector<HintCell> affected = {pivot, wingA, wingB};
        bool hasElimination = false;
        for (int row = 0; row < kGridSize; ++row) {
          for (int col = 0; col < kGridSize; ++col) {
            if ((row == pivot.row && col == pivot.col) || (row == wingA.row && col == wingA.col) ||
                (row == wingB.row && col == wingB.col)) {
              continue;
            }
            if (candidates[row][col].test(zDigit) && SharesUnit(row, col, pivot.row, pivot.col) &&
                SeesBoth({row, col}, wingA, wingB)) {
              AddUniqueCell(affected, row, col);
              hasElimination = true;
            }
          }
        }
        if (hasElimination) {
          return MakeHint("XYZ-Wing", affected, ExtractDigits(pivotMask));
        }
      }
    }
  }

  return {};
}

Hint DetectUniqueRectangleType1(const CandidateGrid& candidates) {
  for (int row1 = 0; row1 < 8; ++row1) {
    for (int row2 = row1 + 1; row2 < 9; ++row2) {
      for (int col1 = 0; col1 < 8; ++col1) {
        for (int col2 = col1 + 1; col2 < 9; ++col2) {
          if (row1 / 3 == row2 / 3 || col1 / 3 == col2 / 3) {
            continue;
          }
          const HintCell a{row1, col1};
          const HintCell b{row1, col2};
          const HintCell c{row2, col1};
          const HintCell d{row2, col2};
          const std::bitset<9> maskA = candidates[a.row][a.col];
          const std::bitset<9> maskB = candidates[b.row][b.col];
          const std::bitset<9> maskC = candidates[c.row][c.col];
          const std::bitset<9> maskD = candidates[d.row][d.col];
          const std::bitset<9> common = maskA & maskB & maskC & maskD;
          if (common.count() != 2) {
            continue;
          }

          std::array<std::pair<HintCell, std::bitset<9>>, 4> rect = {{{a, maskA}, {b, maskB}, {c, maskC}, {d, maskD}}};
          int extraCount = 0;
          HintCell extraCell;
          for (const auto& [cell, mask] : rect) {
            if (mask.none()) {
              extraCount = 0;
              break;
            }
            if ((mask & common) != common) {
              extraCount = 0;
              break;
            }
            if (mask != common) {
              ++extraCount;
              extraCell = cell;
            }
          }

          if (extraCount == 1) {
            return MakeHint("Unique Rectangle", {a, b, c, d, extraCell}, ExtractDigits(common));
          }
        }
      }
    }
  }

  return {};
}

Hint DetectSimpleColoring(const CandidateGrid& candidates) {
  for (int digit = 1; digit <= 9; ++digit) {
    std::vector<HintCell> nodes;
    for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
        if (candidates[row][col].test(digit - 1)) {
          nodes.push_back({row, col});
        }
      }
    }

    const int nodeCount = static_cast<int>(nodes.size());
    if (nodeCount < 4) {
      continue;
    }

    std::vector<std::vector<int>> graph(nodeCount);
    auto addStrongLinks = [&](const std::vector<int>& indices) {
      if (indices.size() == 2) {
        graph[indices[0]].push_back(indices[1]);
        graph[indices[1]].push_back(indices[0]);
      }
    };

    for (int row = 0; row < 9; ++row) {
      std::vector<int> indices;
      for (int index = 0; index < nodeCount; ++index) {
        if (nodes[index].row == row) {
          indices.push_back(index);
        }
      }
      addStrongLinks(indices);
    }
    for (int col = 0; col < 9; ++col) {
      std::vector<int> indices;
      for (int index = 0; index < nodeCount; ++index) {
        if (nodes[index].col == col) {
          indices.push_back(index);
        }
      }
      addStrongLinks(indices);
    }
    for (int boxRow = 0; boxRow < 3; ++boxRow) {
      for (int boxCol = 0; boxCol < 3; ++boxCol) {
        std::vector<int> indices;
        for (int index = 0; index < nodeCount; ++index) {
          if (nodes[index].row / 3 == boxRow && nodes[index].col / 3 == boxCol) {
            indices.push_back(index);
          }
        }
        addStrongLinks(indices);
      }
    }

    std::vector<int> color(nodeCount, -1);
    for (int start = 0; start < nodeCount; ++start) {
      if (color[start] != -1 || graph[start].empty()) {
        continue;
      }

      std::vector<int> stack = {start};
      color[start] = 0;
      while (!stack.empty()) {
        const int node = stack.back();
        stack.pop_back();
        for (int next : graph[node]) {
          if (color[next] == -1) {
            color[next] = 1 - color[node];
            stack.push_back(next);
          }
        }
      }

      std::vector<HintCell> colorA;
      std::vector<HintCell> colorB;
      for (int index = 0; index < nodeCount; ++index) {
        if (color[index] == 0) {
          colorA.push_back(nodes[index]);
        } else if (color[index] == 1) {
          colorB.push_back(nodes[index]);
        }
      }

      for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 9; ++col) {
          if (!candidates[row][col].test(digit - 1) || ContainsCell(nodes, row, col)) {
            continue;
          }
          bool seesA = false;
          bool seesB = false;
          for (const HintCell& cell : colorA) {
            seesA = seesA || SharesUnit(row, col, cell.row, cell.col);
          }
          for (const HintCell& cell : colorB) {
            seesB = seesB || SharesUnit(row, col, cell.row, cell.col);
          }
          if (seesA && seesB) {
            std::vector<HintCell> affected = colorA;
            affected.insert(affected.end(), colorB.begin(), colorB.end());
            AddUniqueCell(affected, row, col);
            return MakeHint("Simple Colouring", affected, {digit});
          }
        }
      }
    }
  }

  return {};
}

Hint DetectXYChain(const CandidateGrid& candidates) {
  std::vector<HintCell> bivalueCells;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      if (candidates[row][col].count() == 2) {
        bivalueCells.push_back({row, col});
      }
    }
  }

  const int n = static_cast<int>(bivalueCells.size());

  struct State {
    int cellIdx;
    int exitDigit;  // digit this cell passes forward to the next link
    std::vector<int> chainIndices;
  };

  for (int startIdx = 0; startIdx < n; ++startIdx) {
    const HintCell& startCell = bivalueCells[startIdx];
    const std::bitset<9> startMask = candidates[startCell.row][startCell.col];

    int digitsInStart[2];
    int di = 0;
    for (int d = 0; d < 9 && di < 2; ++d) {
      if (startMask.test(d)) {
        digitsInStart[di++] = d;
      }
    }

    // Try each digit as the one "held back" at the start (the elimination digit).
    for (int pass = 0; pass < 2; ++pass) {
      const int heldDigit = digitsInStart[pass];
      const int startExit = digitsInStart[1 - pass];

      std::vector<State> stack;
      stack.push_back({startIdx, startExit, {startIdx}});

      while (!stack.empty()) {
        State state = stack.back();
        stack.pop_back();

        const HintCell& currentCell = bivalueCells[state.cellIdx];

        for (int nextIdx = 0; nextIdx < n; ++nextIdx) {
          bool inChain = false;
          for (int ci : state.chainIndices) {
            if (ci == nextIdx) {
              inChain = true;
              break;
            }
          }
          if (inChain) {
            continue;
          }

          const HintCell& nextCell = bivalueCells[nextIdx];
          const std::bitset<9> nextMask = candidates[nextCell.row][nextCell.col];

          if (!nextMask.test(state.exitDigit)) {
            continue;
          }
          if (!SharesUnit(currentCell.row, currentCell.col, nextCell.row, nextCell.col)) {
            continue;
          }

          // The digit nextCell exits with is its other candidate.
          int nextExit = -1;
          for (int d = 0; d < 9; ++d) {
            if (d != state.exitDigit && nextMask.test(d)) {
              nextExit = d;
              break;
            }
          }

          std::vector<int> newChain = state.chainIndices;
          newChain.push_back(nextIdx);

          if (nextExit == heldDigit) {
            // Valid chain: startCell and nextCell both contain heldDigit.
            // Y-Wing already handles 3-cell chains; require >= 4 here to avoid
            // relabelling cases already caught by the Y-Wing detector.
            if (static_cast<int>(newChain.size()) < 4) {
              continue;
            }

            std::vector<HintCell> affected;
            std::vector<HintCell> chainCells;
            for (int ci : newChain) {
              const HintCell chainCell = bivalueCells[ci];
              affected.push_back(chainCell);
              chainCells.push_back(chainCell);
            }

            bool hasElimination = false;
            for (int row = 0; row < kGridSize; ++row) {
              for (int col = 0; col < kGridSize; ++col) {
                if (!candidates[row][col].test(heldDigit)) {
                  continue;
                }
                bool partOfChain = false;
                for (int ci : newChain) {
                  if (bivalueCells[ci].row == row && bivalueCells[ci].col == col) {
                    partOfChain = true;
                    break;
                  }
                }
                if (partOfChain) {
                  continue;
                }
                if (SharesUnit(row, col, startCell.row, startCell.col) &&
                    SharesUnit(row, col, nextCell.row, nextCell.col)) {
                  AddUniqueCell(affected, row, col);
                  hasElimination = true;
                }
              }
            }

            if (hasElimination) {
              return MakeHint("XY-Chain", affected, {heldDigit + 1}, {startCell, nextCell},
                              chainCells);
            }
          } else {
            // Chain not yet closed — extend it.
            stack.push_back({nextIdx, nextExit, std::move(newChain)});
          }
        }
      }
    }
  }

  return {};
}

struct ForcingBranchResult {
  bool contradiction = false;
  Grid grid{};
  CandidateGrid candidates{};
};

bool HasContradiction(const Grid& grid) {
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const int value = grid[row][col].value;
      if (value != 0 && IsConflict(grid, row, col, value)) {
        return true;
      }
      if (value == 0 && !ComputeCandidates(grid, row, col).any()) {
        return true;
      }
    }
  }
  return false;
}

ForcingBranchResult SimulateForcingBranch(const Grid& original, int row, int col, int forcedDigit) {
  ForcingBranchResult result;
  result.grid = original;

  result.grid[row][col].value = forcedDigit;
  result.grid[row][col].pencil.reset();
  RemoveDigitFromPeerPencils(result.grid, row, col, forcedDigit);

  if (HasContradiction(result.grid)) {
    result.contradiction = true;
    return result;
  }

  while (true) {
    bool changed = false;

    const int nakedPlaced = ApplyNakedSingles(result.grid);
    if (nakedPlaced > 0) {
      changed = true;
    }

    const int hiddenPlaced = ApplyHiddenSingles(result.grid);
    if (hiddenPlaced > 0) {
      changed = true;
    }

    if (HasContradiction(result.grid)) {
      result.contradiction = true;
      return result;
    }

    if (!changed) {
      break;
    }
  }

  result.candidates = BuildCandidateGrid(result.grid);
  return result;
}

Hint DetectForcingChains(const Grid& grid, const CandidateGrid& candidates) {
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      if (candidates[row][col].count() != 2) {
        continue;
      }

      std::vector<int> pivotDigits;
      for (int d = 0; d < 9; ++d) {
        if (candidates[row][col].test(d)) {
          pivotDigits.push_back(d + 1);
        }
      }

      if (pivotDigits.size() != 2) {
        continue;
      }

      const ForcingBranchResult left = SimulateForcingBranch(grid, row, col, pivotDigits[0]);
      const ForcingBranchResult right = SimulateForcingBranch(grid, row, col, pivotDigits[1]);

      if (left.contradiction && right.contradiction) {
        continue;
      }

      if (left.contradiction != right.contradiction) {
        const int forced = left.contradiction ? pivotDigits[1] : pivotDigits[0];
        return MakeHint("Forcing Chains", {{row, col}}, {forced});
      }

      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          if (r == row && c == col) {
            continue;
          }

          if (grid[r][c].value == 0 && left.grid[r][c].value != 0 && left.grid[r][c].value == right.grid[r][c].value) {
            return MakeHint("Forcing Chains", {{row, col}, {r, c}}, {left.grid[r][c].value});
          }

          if (grid[r][c].value != 0) {
            continue;
          }

          for (int d = 0; d < 9; ++d) {
            if (!candidates[r][c].test(d)) {
              continue;
            }
            if (!left.candidates[r][c].test(d) && !right.candidates[r][c].test(d)) {
              return MakeHint("Forcing Chains", {{row, col}, {r, c}}, {d + 1});
            }
          }
        }
      }
    }
  }

  return {};
}

Hint GenerateHint(const Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);

  const std::array<Hint (*)(const CandidateGrid&), 10> detectors = {
      DetectNakedSingles,
      DetectHiddenSingles,
      DetectPointingPairs,
      DetectBoxLineReduction,
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 2, "Naked Pair"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 2, "Hidden Pair"); },
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 3, "Naked Triple"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 3, "Hidden Triple"); },
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 4, "Naked Quad"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 4, "Hidden Quad"); },
  };

  for (auto detector : detectors) {
    Hint hint = detector(candidates);
    if (hint.IsValid()) {
      return hint;
    }
  }

  Hint hint = DetectBlockBlockInteraction(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectFish(candidates, 2, "X-Wing");
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectUniqueRectangleType1(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectXYWing(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectSimpleColoring(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectFish(candidates, 3, "Swordfish");
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectXYZWing(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectXYChain(candidates);
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectFish(candidates, 4, "Jellyfish");
  if (hint.IsValid()) {
    return hint;
  }

  hint = DetectForcingChains(grid, candidates);
  if (hint.IsValid()) {
    return hint;
  }

  Hint emptyHint;
  emptyHint.techniqueName = "No supported Sadman hint available";
  return emptyHint;
}

bool IsInsideGrid(int row, int col) {
  return row >= 0 && row < kGridSize && col >= 0 && col < kGridSize;
}

bool ValidateForcingChainsHint(const Grid& grid,
                               const CandidateGrid& candidates,
                               const Hint& hint) {
  if (hint.techniqueName != "Forcing Chains") {
    return true;
  }
  if (hint.affectedCells.empty() || hint.involvedDigits.empty()) {
    return false;
  }

  const HintCell pivot = hint.affectedCells.front();
  if (!IsInsideGrid(pivot.row, pivot.col)) {
    return false;
  }
  if (candidates[pivot.row][pivot.col].count() != 2) {
    return false;
  }

  std::vector<int> pivotDigits;
  for (int d = 0; d < 9; ++d) {
    if (candidates[pivot.row][pivot.col].test(d)) {
      pivotDigits.push_back(d + 1);
    }
  }
  if (pivotDigits.size() != 2) {
    return false;
  }

  const ForcingBranchResult left = SimulateForcingBranch(grid, pivot.row, pivot.col, pivotDigits[0]);
  const ForcingBranchResult right = SimulateForcingBranch(grid, pivot.row, pivot.col, pivotDigits[1]);

  if (hint.affectedCells.size() == 1) {
    if (hint.involvedDigits.size() != 1) {
      return false;
    }
    const int forced = hint.involvedDigits[0];
    if (forced != pivotDigits[0] && forced != pivotDigits[1]) {
      return false;
    }
    const bool leftForces = left.contradiction && !right.contradiction && forced == pivotDigits[1];
    const bool rightForces = right.contradiction && !left.contradiction && forced == pivotDigits[0];
    return leftForces || rightForces;
  }

  const HintCell target = hint.affectedCells[1];
  if (!IsInsideGrid(target.row, target.col)) {
    return false;
  }
  if (hint.involvedDigits.size() != 1) {
    return false;
  }
  const int digit = hint.involvedDigits[0];
  if (digit < 1 || digit > 9) {
    return false;
  }

  const bool commonPlacement =
      grid[target.row][target.col].value == 0 && left.grid[target.row][target.col].value == digit &&
      right.grid[target.row][target.col].value == digit;

  const bool commonElimination =
      grid[target.row][target.col].value == 0 && candidates[target.row][target.col].test(digit - 1) &&
      !left.candidates[target.row][target.col].test(digit - 1) &&
      !right.candidates[target.row][target.col].test(digit - 1);

  return commonPlacement || commonElimination;
}

int RunHintSelfChecks() {
  std::random_device rd;
  std::mt19937 rng(rd());

  int checkedStates = 0;
  int blockHits = 0;
  int forcingHits = 0;
  int failures = 0;

  const std::array<Difficulty, 4> difficulties = {
      Difficulty::kHard,
      Difficulty::kVeryHard,
      Difficulty::kFiendish,
      Difficulty::kDiabolical,
  };

  for (int puzzleIndex = 0; puzzleIndex < 36; ++puzzleIndex) {
    const Difficulty diff = difficulties[puzzleIndex % static_cast<int>(difficulties.size())];
    const Puzzle puzzle = GeneratePuzzleWithDifficulty(rng, diff);
    Puzzle solution{};
    if (!ComputeSolutionFromGivens(puzzle, solution)) {
      continue;
    }

    std::vector<HintCell> empties;
    for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
        if (puzzle[row][col] == 0) {
          empties.push_back({row, col});
        }
      }
    }

    for (int sample = 0; sample < 12; ++sample) {
      Grid grid = BuildGrid(puzzle);
      std::vector<HintCell> order = empties;
      std::shuffle(order.begin(), order.end(), rng);

      const int fillCount = static_cast<int>((order.size() * sample) / 12);
      for (int i = 0; i < fillCount; ++i) {
        const HintCell cell = order[i];
        grid[cell.row][cell.col].value = solution[cell.row][cell.col];
        grid[cell.row][cell.col].pencil.reset();
      }

      if ((rng() % 2) == 0) {
        ApplyNakedSingles(grid);
      }
      if ((rng() % 2) == 0) {
        ApplyHiddenSingles(grid);
      }
      ApplyAutoPencil(grid);

      const CandidateGrid candidates = BuildCandidateGrid(grid);
      ++checkedStates;

      const Hint blockHint = DetectBlockBlockInteraction(candidates);
      if (blockHint.IsValid()) {
        ++blockHits;
        if (blockHint.techniqueName != "Block/Block Interaction" || blockHint.involvedDigits.size() != 1 ||
            blockHint.affectedCells.size() < 3) {
          ++failures;
        }
      }

      const Hint forcingHint = DetectForcingChains(grid, candidates);
      if (forcingHint.IsValid()) {
        ++forcingHits;
        if (!ValidateForcingChainsHint(grid, candidates, forcingHint)) {
          ++failures;
        }
      }

      const Hint generated = GenerateHint(grid);
      if (generated.techniqueName == "Forcing Chains" && !ValidateForcingChainsHint(grid, candidates, generated)) {
        ++failures;
      }
      if (generated.techniqueName == "Block/Block Interaction" &&
          (generated.involvedDigits.size() != 1 || generated.affectedCells.size() < 3)) {
        ++failures;
      }
    }
  }

  std::cout << "Self-check states: " << checkedStates << "\n";
  std::cout << "Block/Block hits: " << blockHits << "\n";
  std::cout << "Forcing Chains hits: " << forcingHits << "\n";
  std::cout << "Consistency failures: " << failures << "\n";

  if (blockHits == 0 || forcingHits == 0) {
    std::cout << "Warning: one or more target techniques were not observed in sampled states.\n";
  }

  return failures == 0 ? 0 : 1;
}

int ApplyAutoPencil(Grid& grid) {
  int changedCells = 0;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      Cell& cell = grid[row][col];
      std::bitset<9> next;
      if (cell.value == 0) {
        next = ComputeCandidates(grid, row, col);
      }
      if (cell.pencil != next) {
        cell.pencil = next;
        ++changedCells;
      }
    }
  }
  return changedCells;
}

int ApplyNakedSingles(Grid& grid) {
  int placements = 0;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      Cell& cell = grid[row][col];
      if (cell.value != 0) {
        continue;
      }

      const std::bitset<9> candidates = ComputeCandidates(grid, row, col);
      if (candidates.count() != 1) {
        continue;
      }

      for (int digit = 1; digit <= 9; ++digit) {
        if (candidates.test(digit - 1)) {
          cell.value = digit;
          cell.pencil.reset();
          RemoveDigitFromPeerPencils(grid, row, col, digit);
          ++placements;
          break;
        }
      }
    }
  }
  return placements;
}

int ApplyHiddenSingles(Grid& grid) {
  int placements = 0;

  for (int row = 0; row < kGridSize; ++row) {
    for (int digit = 1; digit <= 9; ++digit) {
      int count = 0;
      int matchCol = -1;
      for (int col = 0; col < kGridSize; ++col) {
        if (grid[row][col].value != 0) {
          continue;
        }
        if (ComputeCandidates(grid, row, col).test(digit - 1)) {
          ++count;
          matchCol = col;
        }
      }
      if (count == 1 && grid[row][matchCol].value == 0) {
        grid[row][matchCol].value = digit;
        grid[row][matchCol].pencil.reset();
        RemoveDigitFromPeerPencils(grid, row, matchCol, digit);
        ++placements;
      }
    }
  }

  for (int col = 0; col < kGridSize; ++col) {
    for (int digit = 1; digit <= 9; ++digit) {
      int count = 0;
      int matchRow = -1;
      for (int row = 0; row < kGridSize; ++row) {
        if (grid[row][col].value != 0) {
          continue;
        }
        if (ComputeCandidates(grid, row, col).test(digit - 1)) {
          ++count;
          matchRow = row;
        }
      }
      if (count == 1 && grid[matchRow][col].value == 0) {
        grid[matchRow][col].value = digit;
        grid[matchRow][col].pencil.reset();
        RemoveDigitFromPeerPencils(grid, matchRow, col, digit);
        ++placements;
      }
    }
  }

  for (int boxRow = 0; boxRow < 3; ++boxRow) {
    for (int boxCol = 0; boxCol < 3; ++boxCol) {
      const int startRow = boxRow * 3;
      const int startCol = boxCol * 3;
      for (int digit = 1; digit <= 9; ++digit) {
        int count = 0;
        int foundRow = -1;
        int foundCol = -1;
        for (int row = startRow; row < startRow + 3; ++row) {
          for (int col = startCol; col < startCol + 3; ++col) {
            if (grid[row][col].value != 0) {
              continue;
            }
            if (ComputeCandidates(grid, row, col).test(digit - 1)) {
              ++count;
              foundRow = row;
              foundCol = col;
            }
          }
        }
        if (count == 1 && grid[foundRow][foundCol].value == 0) {
          grid[foundRow][foundCol].value = digit;
          grid[foundRow][foundCol].pencil.reset();
          RemoveDigitFromPeerPencils(grid, foundRow, foundCol, digit);
          ++placements;
        }
      }
    }
  }

  return placements;
}

int AutoSolveNakedSingles(Grid& grid) {
  int totalPlacements = 0;
  while (true) {
    const int placed = ApplyNakedSingles(grid);
    if (placed == 0) {
      break;
    }
    totalPlacements += placed;
  }
  ApplyAutoPencil(grid);
  return totalPlacements;
}

int AutoSolveHiddenSingles(Grid& grid) {
  int totalPlacements = 0;
  while (true) {
    const int placed = ApplyHiddenSingles(grid);
    if (placed == 0) {
      break;
    }
    totalPlacements += placed;
  }
  ApplyAutoPencil(grid);
  return totalPlacements;
}

bool SharesUnit(int r1, int c1, int r2, int c2) {
  if (r1 == r2 || c1 == c2) {
    return true;
  }
  return (r1 / 3 == r2 / 3) && (c1 / 3 == c2 / 3);
}

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
};

static Difficulty ParseDifficulty(const std::string& s) {
  if (s == "Simple") return Difficulty::kSimple;
  if (s == "Easy") return Difficulty::kEasy;
  if (s == "Mild") return Difficulty::kMild;
  if (s == "Moderate") return Difficulty::kModerate;
  if (s == "Hard") return Difficulty::kHard;
  if (s == "Very Hard") return Difficulty::kVeryHard;
  if (s == "Fiendish") return Difficulty::kFiendish;
  if (s == "Diabolical") return Difficulty::kDiabolical;
  return Difficulty::kHard;
}

static InputMode ParseMode(const std::string& s) {
  if (s == "Pencil") return InputMode::kPencil;
  if (s == "Color") return InputMode::kColor;
  return InputMode::kDigit;
}

SnapshotLoadResult DeserializeSnapshot(const std::string& text) {
  SnapshotLoadResult result;

  std::istringstream stream(text);
  std::string line;

  // Verify header
  if (!std::getline(stream, line) || line != "format=sudoku-studio-v1") {
    result.errorMessage = "Missing or wrong format header. Expected 'format=sudoku-studio-v1' on first line.";
    return result;
  }

  Grid grid{};
  Hint hint;

  enum class Section { kNone, kGivens, kValues, kPencils, kColors, kHintCells, kConflicts };
  Section section = Section::kNone;
  int boardRow = 0;

  // Tracks which rows of givens/values have been read
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
        result.difficulty = ParseDifficulty(value);
      } else if (key == "mode") {
        result.mode = ParseMode(value);
      } else if (key == "selected_row") {
        result.selectedRow = std::max(0, std::stoi(value) - 1);
      } else if (key == "selected_col") {
        result.selectedCol = std::max(0, std::stoi(value) - 1);
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
  return result;
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

}  // namespace

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
