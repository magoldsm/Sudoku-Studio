#include "hints_candidates.h"

#include "sudoku_core.h"

#include <array>
#include <bitset>
#include <iostream>
#include <vector>

namespace sudoku {

CandidateGrid BuildCandidateGrid(const Grid& grid) {
  CandidateGrid candidates{};
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const std::bitset<9> legalCandidates = ComputeCandidates(grid, row, col);
      if (grid[row][col].value != 0) {
        candidates[row][col].reset();
        continue;
      }

      // Keep user-visible notes when present, but always clamp to legal candidates.
      if (grid[row][col].pencil.any()) {
        candidates[row][col] = legalCandidates & grid[row][col].pencil;
      } else {
        candidates[row][col] = legalCandidates;
      }
    }
  }
  return candidates;
}

CandidateGrid BuildFullCandidateGrid(const Grid& grid) {
  CandidateGrid candidates{};
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      if (grid[row][col].value != 0) {
        candidates[row][col].reset();
      } else {
        candidates[row][col] = ComputeCandidates(grid, row, col);
      }
    }
  }

  return candidates;
}

std::vector<UnitCells> BuildAllUnits() {
  std::vector<UnitCells> units;
  // 9 rows + 9 columns + 9 boxes.
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

void AddUniqueCell(std::vector<HintCell>& cells, int row, int col) {
  if (!ContainsCell(cells, row, col)) {
    cells.push_back({row, col});
  }
}
bool ContainsCell(const std::vector<HintCell>& cells, int row, int col) {
  for (const HintCell& cell : cells) {
    if (cell.row == row && cell.col == col) {
      return true;
    }
  }
  return false;
}

bool SeesBoth(const HintCell& target, const HintCell& left, const HintCell& right) {
  return SharesUnit(target.row, target.col, left.row, left.col) &&
         SharesUnit(target.row, target.col, right.row, right.col);
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

}  // namespace sudoku
