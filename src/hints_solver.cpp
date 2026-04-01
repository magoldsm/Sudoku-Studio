#include "hints_solver.h"

#include <array>
#include <bitset>
#include <iostream>
#include <random>
#include <vector>

namespace sudoku {
namespace {

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

  for (int index = start; index <= static_cast<int>(values.size()) -
                                   (choose - static_cast<int>(current.size()));
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

}  // namespace

bool ContainsCell(const std::vector<HintCell>& cells, int row, int col) {
  for (const HintCell& cell : cells) {
    if (cell.row == row && cell.col == col) {
      return true;
    }
  }
  return false;
}

bool SharesUnit(int r1, int c1, int r2, int c2) {
  if (r1 == r2 || c1 == c2) {
    return true;
  }
  return (r1 / 3 == r2 / 3) && (c1 / 3 == c2 / 3);
}

// Extracted from main.cpp during modular refactor.
// Hint detection, forcing-chain validation, and solver helpers.
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
  std::vector<HintCell> deductionPath;  // Cells deduced during solving
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
  result.deductionPath.push_back({row, col});

  if (HasContradiction(result.grid)) {
    result.contradiction = true;
    return result;
  }

  // Propagate with simple deterministic rules only; stop at first fixed point.
  // Track ALL cells that were newly solved - this shows the cascade
  while (true) {
    bool changed = false;
    Grid gridBeforeNaked = result.grid;

    const int nakedPlaced = ApplyNakedSingles(result.grid);
    if (nakedPlaced > 0) {
      changed = true;
      // Record all cells that were newly solved
      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          if (gridBeforeNaked[r][c].value == 0 && result.grid[r][c].value != 0) {
            AddUniqueCell(result.deductionPath, r, c);
          }
        }
      }
    }

    Grid gridBeforeHidden = result.grid;
    const int hiddenPlaced = ApplyHiddenSingles(result.grid);
    if (hiddenPlaced > 0) {
      changed = true;
      // Record all cells that were newly solved
      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          if (gridBeforeHidden[r][c].value == 0 && result.grid[r][c].value != 0) {
            AddUniqueCell(result.deductionPath, r, c);
          }
        }
      }
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
        // Skip contradiction-only hints - they don't show a chain structure
        // Only return hints where multiple cells are involved in the deduction
        continue;
      }

      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          if (r == row && c == col) {
            continue;
          }

          if (grid[r][c].value == 0 && left.grid[r][c].value != 0 && left.grid[r][c].value == right.grid[r][c].value) {
            Hint hint;
            hint.techniqueName = "Forcing Chains";
            hint.affectedCells = {{row, col}, {r, c}};
            hint.involvedDigits = {left.grid[r][c].value};
            hint.chainEndpoints = {{row, col}, {r, c}};
            // Build chain from deduction path - use the first branch's path
            // (both branches lead to the same conclusion, so either works)
            hint.chainCells = left.deductionPath;
            AddUniqueCell(hint.chainCells, r, c);  // Add the final conclusion cell
            return hint;
          }

          if (grid[r][c].value != 0) {
            continue;
          }

          for (int d = 0; d < 9; ++d) {
            if (!candidates[r][c].test(d)) {
              continue;
            }
            if (!left.candidates[r][c].test(d) && !right.candidates[r][c].test(d)) {
              Hint hint;
              hint.techniqueName = "Forcing Chains";
              hint.affectedCells = {{row, col}, {r, c}};
              hint.involvedDigits = {d + 1};
              hint.chainEndpoints = {{row, col}, {r, c}};
              // Build chain from deduction path
              hint.chainCells = left.deductionPath;
              AddUniqueCell(hint.chainCells, r, c);  // Add the cell with eliminated candidate
              return hint;
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

  // Ordered from easier/common techniques to heavier analysis.
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

  for (int i = 0; i < static_cast<int>(detectors.size()); ++i) {
    Hint hint = detectors[i](candidates);
    if (!hint.IsValid()) continue;
    // Validate hidden subset hints: they are only actionable if there are
    // actual pencil marks to eliminate (detector uses CandidateGrid which
    // includes legal candidates for cells that have no pencil marks set).
    if (hint.techniqueName == "Hidden Pair") {
      Grid testGrid = grid;
      if (ApplyHiddenPairs(testGrid) == 0) continue;
    } else if (hint.techniqueName == "Hidden Triple") {
      Grid testGrid = grid;
      if (ApplyHiddenTriples(testGrid) == 0) continue;
    } else if (hint.techniqueName == "Hidden Quad") {
      // No dedicated apply function for quads; validate by checking
      // that affected cells actually have pencil marks to eliminate.
      bool actionable = false;
      for (const auto& hc : hint.affectedCells) {
        if (grid[hc.row][hc.col].pencil.any()) { actionable = true; break; }
      }
      if (!actionable) continue;
    }
    return hint;
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

  // Single-cell forcing means one branch contradicted, so the other digit is forced.
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

  // Two-cell forcing means a shared outcome/elimination at target across branches.
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

      const Hint generated = GenerateHint(grid);
      if (generated.techniqueName == "Block/Block Interaction" &&
          (generated.involvedDigits.size() != 1 || generated.affectedCells.size() < 3)) {
        ++failures;
      }
    }
  }

  std::cout << "Self-check states: " << checkedStates << "\n";
  std::cout << "Block/Block hits: " << blockHits << "\n";
  std::cout << "Consistency failures: " << failures << "\n";

  if (blockHits == 0) {
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
        const std::bitset<9> legalCandidates = ComputeCandidates(grid, row, col);
        if (cell.pencil.any()) {
          // User has existing marks: preserve them, but prune illegal ones
          next = cell.pencil & legalCandidates;
        } else {
          // No marks yet: fill with legal candidates
          next = legalCandidates;
        }
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

      // Use user's pencil marks if available, otherwise compute legal candidates
      const std::bitset<9> candidates =
          cell.pencil.any() ? cell.pencil : ComputeCandidates(grid, row, col);
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
        // Use user's pencil marks if available, otherwise compute legal candidates
        const std::bitset<9> candidates =
            grid[row][col].pencil.any() ? grid[row][col].pencil : ComputeCandidates(grid, row, col);
        if (candidates.test(digit - 1)) {
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
        // Use user's pencil marks if available, otherwise compute legal candidates
        const std::bitset<9> candidates =
            grid[row][col].pencil.any() ? grid[row][col].pencil : ComputeCandidates(grid, row, col);
        if (candidates.test(digit - 1)) {
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
            // Use user's pencil marks if available, otherwise compute legal candidates
            const std::bitset<9> candidates =
                grid[row][col].pencil.any() ? grid[row][col].pencil : ComputeCandidates(grid, row, col);
            if (candidates.test(digit - 1)) {
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

// Solver for pointing pairs/triples (intersection removal)
int ApplyPointingPairs(Grid& grid) {
  bool madeChanges = false;

  // For each box, check if a candidate is confined to one row/column
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      for (int digit = 1; digit <= 9; ++digit) {
        std::vector<std::pair<int, int>> positions;

        for (int localR = 0; localR < 3; ++localR) {
          for (int localC = 0; localC < 3; ++localC) {
            const int r = boxR * 3 + localR;
            const int c = boxC * 3 + localC;
            if (grid[r][c].pencil.test(digit - 1)) {
              positions.push_back({r, c});
            }
          }
        }

        if (positions.empty()) continue;

        // Check if all positions share the same row
        bool sameRow = true;
        const int row = positions[0].first;
        for (const auto& pos : positions) {
          if (pos.first != row) {
            sameRow = false;
            break;
          }
        }

        if (sameRow) {
          // Remove digit from rest of row
          for (int c = 0; c < kGridSize; ++c) {
            if (c < boxC * 3 || c >= (boxC + 1) * 3) {
              if (grid[row][c].pencil.test(digit - 1)) {
                grid[row][c].pencil.reset(digit - 1);
                madeChanges = true;
              }
            }
          }
        }

        // Check if all positions share the same column
        bool sameCol = true;
        const int col = positions[0].second;
        for (const auto& pos : positions) {
          if (pos.second != col) {
            sameCol = false;
            break;
          }
        }

        if (sameCol) {
          // Remove digit from rest of column
          for (int r = 0; r < kGridSize; ++r) {
            if (r < boxR * 3 || r >= (boxR + 1) * 3) {
              if (grid[r][col].pencil.test(digit - 1)) {
                grid[r][col].pencil.reset(digit - 1);
                madeChanges = true;
              }
            }
          }
        }
      }
    }
  }

  // Return 1 if technique was applied, 0 otherwise
  return madeChanges ? 1 : 0;
}

// Solver for box/line reduction
int ApplyBoxLineReduction(Grid& grid) {
  bool madeChanges = false;

  // If a candidate appears only in one box within a row/column,
  // remove it from the box outside that line

  for (int digit = 1; digit <= 9; ++digit) {
    // Check rows
    for (int r = 0; r < kGridSize; ++r) {
      std::vector<int> boxesWithDigit;

      for (int boxC = 0; boxC < 3; ++boxC) {
        bool found = false;
        for (int c = boxC * 3; c < (boxC + 1) * 3; ++c) {
          if (grid[r][c].pencil.test(digit - 1)) {
            found = true;
            break;
          }
        }
        if (found) boxesWithDigit.push_back(boxC);
      }

      // If digit in row appears in only one box column, remove from rest of box
      if (boxesWithDigit.size() == 1) {
        const int boxC = boxesWithDigit[0];
        const int boxR = r / 3;

        for (int localR = 0; localR < 3; ++localR) {
          for (int localC = 0; localC < 3; ++localC) {
            const int checkR = boxR * 3 + localR;
            const int checkC = boxC * 3 + localC;
            if (checkR != r && grid[checkR][checkC].pencil.test(digit - 1)) {
              grid[checkR][checkC].pencil.reset(digit - 1);
              madeChanges = true;
            }
          }
        }
      }
    }

    // Check columns
    for (int c = 0; c < kGridSize; ++c) {
      std::vector<int> boxesWithDigit;

      for (int boxR = 0; boxR < 3; ++boxR) {
        bool found = false;
        for (int r = boxR * 3; r < (boxR + 1) * 3; ++r) {
          if (grid[r][c].pencil.test(digit - 1)) {
            found = true;
            break;
          }
        }
        if (found) boxesWithDigit.push_back(boxR);
      }

      // If digit in column appears in only one box row, remove from rest of box
      if (boxesWithDigit.size() == 1) {
        const int boxR = boxesWithDigit[0];
        const int boxC = c / 3;

        for (int localR = 0; localR < 3; ++localR) {
          for (int localC = 0; localC < 3; ++localC) {
            const int checkR = boxR * 3 + localR;
            const int checkC = boxC * 3 + localC;
            if (checkC != c && grid[checkR][checkC].pencil.test(digit - 1)) {
              grid[checkR][checkC].pencil.reset(digit - 1);
              madeChanges = true;
            }
          }
        }
      }
    }
  }

  // Return 1 if technique was applied, 0 otherwise
  return madeChanges ? 1 : 0;
}

// Solver for naked pairs
int ApplyNakedPairs(Grid& grid) {
  bool madeChanges = false;

  // Find pairs in rows, columns, and boxes
  for (int r = 0; r < kGridSize; ++r) {
    std::vector<std::pair<long unsigned int, std::pair<int, int>>> pairCells;

    for (int c = 0; c < kGridSize; ++c) {
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() == 2) {
        pairCells.push_back({grid[r][c].pencil.to_ulong(), {r, c}});
      }
    }

    // Check for duplicate pairs in row
    for (size_t i = 0; i < pairCells.size(); ++i) {
      for (size_t j = i + 1; j < pairCells.size(); ++j) {
        if (pairCells[i].first == pairCells[j].first) {
          // Found a pair, remove these candidates from other cells in row
          const auto& pair = pairCells[i].first;
          for (int c = 0; c < kGridSize; ++c) {
            if (c != pairCells[i].second.second && c != pairCells[j].second.second) {
              for (int d = 0; d < 9; ++d) {
                if (((pair >> d) & 1) && grid[r][c].pencil.test(d)) {
                  grid[r][c].pencil.reset(d);
                  madeChanges = true;
                }
              }
            }
          }
        }
      }
    }
  }

  // Return 1 if technique was applied, 0 otherwise
  return madeChanges ? 1 : 0;
}

// Solver for hidden pairs
int ApplyHiddenPairs(Grid& grid) {
  bool madeChanges = false;

  // Check rows
  for (int r = 0; r < kGridSize; ++r) {
    // For each pair of digits, find which cells they appear in
    for (int d1 = 1; d1 <= 9; ++d1) {
      for (int d2 = d1 + 1; d2 <= 9; ++d2) {
        std::vector<int> cellsWithPair;
        for (int c = 0; c < kGridSize; ++c) {
          if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
              grid[r][c].pencil.test(d2 - 1)) {
            cellsWithPair.push_back(c);
          }
        }

        // If exactly 2 cells contain this pair (and no other cells have both digits)
        if (cellsWithPair.size() == 2) {
          // Check if d1 and d2 appear ONLY in these 2 cells
          bool d1OnlyInPair = true;
          bool d2OnlyInPair = true;
          for (int c = 0; c < kGridSize; ++c) {
            if (c != cellsWithPair[0] && c != cellsWithPair[1]) {
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1)) {
                d1OnlyInPair = false;
              }
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d2 - 1)) {
                d2OnlyInPair = false;
              }
            }
          }

          // If both digits appear only in these 2 cells, remove all other candidates
          if (d1OnlyInPair && d2OnlyInPair) {
            for (int c : cellsWithPair) {
              for (int d = 1; d <= 9; ++d) {
                if (d != d1 && d != d2 && grid[r][c].pencil.test(d - 1)) {
                  grid[r][c].pencil.reset(d - 1);
                  madeChanges = true;
                }
              }
            }
          }
        }
      }
    }
  }

  // Check columns
  for (int c = 0; c < kGridSize; ++c) {
    for (int d1 = 1; d1 <= 9; ++d1) {
      for (int d2 = d1 + 1; d2 <= 9; ++d2) {
        std::vector<int> cellsWithPair;
        for (int r = 0; r < kGridSize; ++r) {
          if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
              grid[r][c].pencil.test(d2 - 1)) {
            cellsWithPair.push_back(r);
          }
        }

        if (cellsWithPair.size() == 2) {
          bool d1OnlyInPair = true;
          bool d2OnlyInPair = true;
          for (int r = 0; r < kGridSize; ++r) {
            if (r != cellsWithPair[0] && r != cellsWithPair[1]) {
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1)) {
                d1OnlyInPair = false;
              }
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d2 - 1)) {
                d2OnlyInPair = false;
              }
            }
          }

          if (d1OnlyInPair && d2OnlyInPair) {
            for (int r : cellsWithPair) {
              for (int d = 1; d <= 9; ++d) {
                if (d != d1 && d != d2 && grid[r][c].pencil.test(d - 1)) {
                  grid[r][c].pencil.reset(d - 1);
                  madeChanges = true;
                }
              }
            }
          }
        }
      }
    }
  }

  return madeChanges ? 1 : 0;
}

// Solver for naked triples
int ApplyNakedTriples(Grid& grid) {
  bool madeChanges = false;

  // Check rows
  for (int r = 0; r < kGridSize; ++r) {
    std::vector<int> tripleCells;
    // Find all cells with exactly 2 or 3 candidates
    for (int c = 0; c < kGridSize; ++c) {
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 &&
          grid[r][c].pencil.count() <= 3) {
        tripleCells.push_back(c);
      }
    }

    // Check all combinations of 3 cells
    for (size_t i = 0; i < tripleCells.size(); ++i) {
      for (size_t j = i + 1; j < tripleCells.size(); ++j) {
        for (size_t k = j + 1; k < tripleCells.size(); ++k) {
          // Union of candidates in these 3 cells
          std::bitset<9> unionCandidates = grid[r][tripleCells[i]].pencil |
                                           grid[r][tripleCells[j]].pencil |
                                           grid[r][tripleCells[k]].pencil;

          // If union has exactly 3 candidates, we have a naked triple
          if (unionCandidates.count() == 3) {
            // Remove these 3 candidates from all other cells
            for (int c = 0; c < kGridSize; ++c) {
              if (c != tripleCells[i] && c != tripleCells[j] && c != tripleCells[k]) {
                for (int d = 0; d < 9; ++d) {
                  if (unionCandidates.test(d) && grid[r][c].pencil.test(d)) {
                    grid[r][c].pencil.reset(d);
                    madeChanges = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Check columns
  for (int c = 0; c < kGridSize; ++c) {
    std::vector<int> tripleCells;
    for (int r = 0; r < kGridSize; ++r) {
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 &&
          grid[r][c].pencil.count() <= 3) {
        tripleCells.push_back(r);
      }
    }

    for (size_t i = 0; i < tripleCells.size(); ++i) {
      for (size_t j = i + 1; j < tripleCells.size(); ++j) {
        for (size_t k = j + 1; k < tripleCells.size(); ++k) {
          std::bitset<9> unionCandidates = grid[tripleCells[i]][c].pencil |
                                           grid[tripleCells[j]][c].pencil |
                                           grid[tripleCells[k]][c].pencil;

          if (unionCandidates.count() == 3) {
            for (int r = 0; r < kGridSize; ++r) {
              if (r != tripleCells[i] && r != tripleCells[j] && r != tripleCells[k]) {
                for (int d = 0; d < 9; ++d) {
                  if (unionCandidates.test(d) && grid[r][c].pencil.test(d)) {
                    grid[r][c].pencil.reset(d);
                    madeChanges = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return madeChanges ? 1 : 0;
}

// Solver for hidden triples
int ApplyHiddenTriples(Grid& grid) {
  bool madeChanges = false;

  // Check rows
  for (int r = 0; r < kGridSize; ++r) {
    // For each combination of 3 digits
    for (int d1 = 1; d1 <= 9; ++d1) {
      for (int d2 = d1 + 1; d2 <= 9; ++d2) {
        for (int d3 = d2 + 1; d3 <= 9; ++d3) {
          // Find cells containing all three digits
          std::vector<int> cellsWithTriple;
          for (int c = 0; c < kGridSize; ++c) {
            if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
                grid[r][c].pencil.test(d2 - 1) && grid[r][c].pencil.test(d3 - 1)) {
              cellsWithTriple.push_back(c);
            }
          }

          // If exactly 3 cells and these digits appear ONLY in these cells
          if (cellsWithTriple.size() == 3) {
            bool allOnlyInTriple = true;
            for (int c = 0; c < kGridSize; ++c) {
              if (c != cellsWithTriple[0] && c != cellsWithTriple[1] &&
                  c != cellsWithTriple[2]) {
                if ((grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1)) ||
                    (grid[r][c].value == 0 && grid[r][c].pencil.test(d2 - 1)) ||
                    (grid[r][c].value == 0 && grid[r][c].pencil.test(d3 - 1))) {
                  allOnlyInTriple = false;
                  break;
                }
              }
            }

            if (allOnlyInTriple) {
              // Remove all other candidates from these 3 cells
              for (int c : cellsWithTriple) {
                for (int d = 1; d <= 9; ++d) {
                  if (d != d1 && d != d2 && d != d3 && grid[r][c].pencil.test(d - 1)) {
                    grid[r][c].pencil.reset(d - 1);
                    madeChanges = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Check columns
  for (int c = 0; c < kGridSize; ++c) {
    for (int d1 = 1; d1 <= 9; ++d1) {
      for (int d2 = d1 + 1; d2 <= 9; ++d2) {
        for (int d3 = d2 + 1; d3 <= 9; ++d3) {
          std::vector<int> cellsWithTriple;
          for (int r = 0; r < kGridSize; ++r) {
            if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
                grid[r][c].pencil.test(d2 - 1) && grid[r][c].pencil.test(d3 - 1)) {
              cellsWithTriple.push_back(r);
            }
          }

          if (cellsWithTriple.size() == 3) {
            bool allOnlyInTriple = true;
            for (int r = 0; r < kGridSize; ++r) {
              if (r != cellsWithTriple[0] && r != cellsWithTriple[1] &&
                  r != cellsWithTriple[2]) {
                if ((grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1)) ||
                    (grid[r][c].value == 0 && grid[r][c].pencil.test(d2 - 1)) ||
                    (grid[r][c].value == 0 && grid[r][c].pencil.test(d3 - 1))) {
                  allOnlyInTriple = false;
                  break;
                }
              }
            }

            if (allOnlyInTriple) {
              for (int r : cellsWithTriple) {
                for (int d = 1; d <= 9; ++d) {
                  if (d != d1 && d != d2 && d != d3 && grid[r][c].pencil.test(d - 1)) {
                    grid[r][c].pencil.reset(d - 1);
                    madeChanges = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return madeChanges ? 1 : 0;
}

// Comprehensive solver using all available techniques
bool SolveComprehensive(Puzzle& puzzle) {
  Grid grid = BuildGrid(puzzle);
  ApplyAutoPencil(grid);

  int maxIterations = 200;
  int iteration = 0;

  while (iteration < maxIterations) {
    bool changed = false;

    // Apply naked singles until stable
    while (true) {
      const int placed = ApplyNakedSingles(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply hidden singles until stable
    while (true) {
      const int placed = ApplyHiddenSingles(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply pointing pairs until stable
    while (true) {
      const int placed = ApplyPointingPairs(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply box/line reduction until stable
    while (true) {
      const int placed = ApplyBoxLineReduction(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply naked pairs until stable
    while (true) {
      const int placed = ApplyNakedPairs(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply hidden pairs until stable
    while (true) {
      const int placed = ApplyHiddenPairs(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply naked triples until stable
    while (true) {
      const int placed = ApplyNakedTriples(grid);
      if (placed == 0) break;
      changed = true;
    }

    // Apply hidden triples until stable
    while (true) {
      const int placed = ApplyHiddenTriples(grid);
      if (placed == 0) break;
      changed = true;
    }

    // If no progress in this iteration, we're done
    if (!changed) {
      break;
    }

    iteration++;
  }

  // Check if fully solved
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      if (grid[r][c].value == 0) {
        return false;  // Not fully solved
      }
      // Check for conflicts
      if (IsConflict(grid, r, c, grid[r][c].value)) {
        return false;  // Invalid solution
      }
    }
  }

  // Convert back to puzzle
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      puzzle[r][c] = grid[r][c].value;
    }
  }

  return true;
}

int GetTechniqueScore(SolvingTechnique technique) {
  switch (technique) {
    case SolvingTechnique::kNakedSingle:
      return 1;      // Trivial
    case SolvingTechnique::kHiddenSingle:
      return 3;      // Requires scanning
    case SolvingTechnique::kPointingPair:
      return 10;     // Significant pattern
    case SolvingTechnique::kBoxLineReduction:
      return 10;
    case SolvingTechnique::kNakedPair:
      return 15;     // Set theory
    case SolvingTechnique::kHiddenPair:
      return 20;
    case SolvingTechnique::kNakedTriple:
      return 30;     // Advanced
    case SolvingTechnique::kHiddenTriple:
      return 40;
  }
  return 0;
}

int SolveAndScore(const Puzzle& puzzle) {
  return SolveAndScoreDetailed(puzzle).totalScore;
}

PuzzleScore SolveAndScoreDetailed(const Puzzle& puzzle) {
  Grid grid = BuildGrid(puzzle);
  PuzzleScore score{};
  constexpr int kMaxIterations = 200;

  ApplyAutoPencil(grid);

  for (int i = 0; i < kMaxIterations; ++i) {
    // Try techniques in priority order — credit only the first that makes progress
    if (ApplyNakedSingles(grid) > 0) {
      score.nakedSingles += GetTechniqueScore(SolvingTechnique::kNakedSingle);
      continue;
    }
    if (ApplyHiddenSingles(grid) > 0) {
      score.hiddenSingles += GetTechniqueScore(SolvingTechnique::kHiddenSingle);
      continue;
    }
    if (ApplyPointingPairs(grid) > 0) {
      score.pointingPairs += GetTechniqueScore(SolvingTechnique::kPointingPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }
    if (ApplyBoxLineReduction(grid) > 0) {
      score.boxLineReductions += GetTechniqueScore(SolvingTechnique::kBoxLineReduction);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }
    if (ApplyNakedPairs(grid) > 0) {
      score.nakedPairs += GetTechniqueScore(SolvingTechnique::kNakedPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }
    if (ApplyHiddenPairs(grid) > 0) {
      score.hiddenPairs += GetTechniqueScore(SolvingTechnique::kHiddenPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }
    if (ApplyNakedTriples(grid) > 0) {
      score.nakedTriples += GetTechniqueScore(SolvingTechnique::kNakedTriple);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }
    if (ApplyHiddenTriples(grid) > 0) {
      score.hiddenTriples += GetTechniqueScore(SolvingTechnique::kHiddenTriple);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      continue;
    }

    break;  // No technique made progress
  }

  score.totalScore = score.nakedSingles + score.hiddenSingles + score.pointingPairs +
                     score.boxLineReductions + score.nakedPairs + score.hiddenPairs +
                     score.nakedTriples + score.hiddenTriples;
  return score;
}

int MeasurePuzzleDifficulty(const Puzzle& puzzle) {
  // Technique levels:
  // 1 = Naked Singles
  // 2 = Hidden Singles
  // 3 = Pointing Pairs / Box-Line Reduction
  // 4 = Naked Pairs
  // 5+ = Advanced techniques (not implemented yet)

  Grid grid = BuildGrid(puzzle);

  // Try level 1: Naked singles only
  {
    Grid testGrid = grid;
    while (true) {
      const int placed = ApplyNakedSingles(testGrid);
      if (placed == 0) break;
    }
    bool complete = true;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        if (testGrid[r][c].value == 0) {
          complete = false;
          break;
        }
      }
      if (!complete) break;
    }
    if (complete) return 1;
  }

  // Try level 2: + Hidden singles
  {
    Grid testGrid = grid;
    int maxIter = 100;
    while (maxIter-- > 0) {
      int placed = 0;
      placed += ApplyNakedSingles(testGrid);
      placed += ApplyHiddenSingles(testGrid);
      if (placed == 0) break;
    }
    bool complete = true;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        if (testGrid[r][c].value == 0) {
          complete = false;
          break;
        }
      }
      if (!complete) break;
    }
    if (complete) return 2;
  }

  // Try level 3: + Pointing pairs / Box-line
  {
    Grid testGrid = grid;
    int maxIter = 100;
    while (maxIter-- > 0) {
      int placed = 0;
      placed += ApplyNakedSingles(testGrid);
      placed += ApplyHiddenSingles(testGrid);
      placed += ApplyPointingPairs(testGrid);
      placed += ApplyBoxLineReduction(testGrid);
      if (placed == 0) break;
    }
    bool complete = true;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        if (testGrid[r][c].value == 0) {
          complete = false;
          break;
        }
      }
      if (!complete) break;
    }
    if (complete) return 3;
  }

  // Try level 4: + Naked pairs
  {
    Grid testGrid = grid;
    int maxIter = 100;
    while (maxIter-- > 0) {
      int placed = 0;
      placed += ApplyNakedSingles(testGrid);
      placed += ApplyHiddenSingles(testGrid);
      placed += ApplyPointingPairs(testGrid);
      placed += ApplyBoxLineReduction(testGrid);
      placed += ApplyNakedPairs(testGrid);
      if (placed == 0) break;
    }
    bool complete = true;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        if (testGrid[r][c].value == 0) {
          complete = false;
          break;
        }
      }
      if (!complete) break;
    }
    if (complete) return 4;
  }

  // If not solvable with current techniques, return 5+
  return 5;
}

}  // namespace sudoku
