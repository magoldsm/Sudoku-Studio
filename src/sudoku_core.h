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
// Candidate grid: for each cell, which digits are legal candidates
using CandidateGrid = std::array<std::array<std::bitset<9>, kGridSize>, kGridSize>;

enum class InputMode {
  kDigit,
  kPencil,
  kColor,
  kCandidateColor,
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

struct DifficultyBand {
  Difficulty difficulty;
  int scoreMin;
  int scoreMax;
};

// Get difficulty bands table
const DifficultyBand* GetDifficultyBands();
constexpr int GetDifficultyBandCount() { return 8; }

// Convert InputMode and Difficulty enums to display strings
const char* ModeName(InputMode mode);
const char* DifficultyName(Difficulty difficulty);
// Parse string representations back to enums
InputMode ParseMode(const char* modeStr);
Difficulty ParseDifficulty(const char* diffStr);

struct Cell {
  int value = 0;
  std::bitset<9> pencil;
  int colorTag = 0;
  std::array<int, 10> candidateColors = {};  // candidateColors[d] = color for digit d (0 = no color)
  bool fixed = false;
};

using Grid = std::array<std::array<Cell, kGridSize>, kGridSize>;

struct HintCell {
  int row = -1;
  int col = -1;

  bool operator==(const HintCell& other) const {
    return row == other.row && col == other.col;
  }
};

// Missing candidate for hint-affected cells (also used in validation)
struct MissingCandidate {
  int row = -1;
  int col = -1;
  std::vector<int> missingDigits;  // Digits 1-9 that are legal but not in user's pencil marks
};

// Validation results for erroneous candidate eliminations
struct ValidationWarning {
  std::vector<MissingCandidate> erasedButLegal;  // Candidates user erased that are still legal
};

struct Hint {
  std::string techniqueName;
  std::vector<HintCell> affectedCells;
  std::vector<int> involvedDigits;
  std::vector<HintCell> chainEndpoints;
  std::vector<HintCell> chainCells;
  // 0 = none, 1 = technique name, 2 = cells highlighted, 3 = digits highlighted.
  int revealPhase = 0;
  ValidationWarning validation;  // Populated when no solving hint found
  // Candidate grid used to detect this hint (ensures apply uses same logic)
  CandidateGrid usedCandidates{};

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

// True if digit can be placed at (row, col) without violating Sudoku rules (on raw Puzzle).
bool IsSafeInPuzzle(const Puzzle& board, int row, int col, int digit);

// True if two coordinates share row, column, or box.
bool SharesUnit(int r1, int c1, int r2, int c2);
bool IsConflict(const Grid& grid, int row, int col, int value);
// True if grid is completely solved with no conflicts.
bool IsSolved(const Grid& grid);
// Computes legal candidates from solved values, independent of visible pencil marks.
std::bitset<9> ComputeCandidates(const Grid& grid, int row, int col);
// Clears the placed digit from all unsolved peers in row/column/box.
void RemoveDigitFromPeerPencils(Grid& grid, int row, int col, int digit);

// Puzzle scoring details
struct PuzzleScore {
  int totalScore = 0;
  int nakedSingles = 0;
  int hiddenSingles = 0;
  int pointingPairs = 0;
  int boxLineReductions = 0;
  int nakedPairs = 0;
  int hiddenPairs = 0;
  int nakedTriples = 0;
  int hiddenTriples = 0;
  int nakedQuads = 0;
  int hiddenQuads = 0;
  int blockBlockInteractions = 0;
  int xWings = 0;
  int uniqueRectangles = 0;
  int yWings = 0;
  int simpleColourings = 0;
  int swordfishes = 0;
  int xyzWings = 0;
  int xyChains = 0;
  int jellyfishes = 0;
  int forcingChains = 0;
};

}  // namespace sudoku
