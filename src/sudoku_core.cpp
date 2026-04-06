#include "sudoku_core.h"

namespace sudoku {

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

InputMode ParseMode(const char* modeStr) {
  if (!modeStr) return InputMode::kDigit;
  if (std::string(modeStr) == "Pencil") return InputMode::kPencil;
  if (std::string(modeStr) == "Color") return InputMode::kColor;
  return InputMode::kDigit;
}

Difficulty ParseDifficulty(const char* diffStr) {
  if (!diffStr) return Difficulty::kHard;
  std::string s(diffStr);
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

const DifficultyBand* GetDifficultyBands() {
  static const DifficultyBand bands[] = {
    {Difficulty::kSimple, 0, 12},
    {Difficulty::kEasy, 8, 30},
    {Difficulty::kMild, 22, 80},
    {Difficulty::kModerate, 60, 200},
    {Difficulty::kHard, 150, 500},
    {Difficulty::kVeryHard, 400, 1200},
    {Difficulty::kFiendish, 900, 3000},
    {Difficulty::kDiabolical, 2000, 100000},
  };
  return bands;
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

namespace {

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

}  // anonymous namespace

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

}  // namespace sudoku
