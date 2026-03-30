#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <random>
#include <string>
#include <vector>

namespace sudoku {

// Shared board geometry constants used by UI layout and solver loops.
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
  // 0 = none, 1 = technique name, 2 = cells highlighted, 3 = digits highlighted.
  int revealPhase = 0;

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

Puzzle GeneratePuzzleWithDifficulty(std::mt19937& rng, Difficulty difficulty);
Grid BuildGrid(const Puzzle& puzzle);
// Solves from givens only; returns false when givens are invalid or unsatisfiable.
bool ComputeSolutionFromGivens(const Puzzle& givens, Puzzle& solution);

// True if two coordinates share row, column, or box.
bool SharesUnit(int r1, int c1, int r2, int c2);
bool IsConflict(const Grid& grid, int row, int col, int value);
// Computes legal candidates from solved values, independent of visible pencil marks.
std::bitset<9> ComputeCandidates(const Grid& grid, int row, int col);
// Clears the placed digit from all unsolved peers in row/column/box.
void RemoveDigitFromPeerPencils(Grid& grid, int row, int col, int digit);

}  // namespace sudoku
