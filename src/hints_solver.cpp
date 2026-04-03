#include "hints_solver.h"

#include <array>
#include <bitset>
#include <iostream>
#include <random>
#include <vector>

namespace sudoku {
namespace {

Hint MakeHint(const char* techniqueName, const std::vector<HintCell>& cells) {
  Hint hint;
  hint.techniqueName = techniqueName;
  hint.affectedCells = cells;
  return hint;
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

        std::vector<HintCell> affected;
        bool hasElimination = false;
        if (sameRow) {
          const int row = boxCells.front().row;
          // Verify digit appears ONLY in this row within the box
          bool appearsOnlyInThisRow = true;
          for (int r = startRow; r < startRow + 3; ++r) {
            if (r == row) continue;
            for (int c = startCol; c < startCol + 3; ++c) {
              if (candidates[r][c].test(digit - 1)) {
                appearsOnlyInThisRow = false;
                break;
              }
            }
            if (!appearsOnlyInThisRow) break;
          }
          if (appearsOnlyInThisRow) {
            // Add pointing cells to show where digit is confined
            for (const HintCell& cell : boxCells) {
              AddUniqueCell(affected, cell.row, cell.col);
            }
            // Add elimination cells
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
        }
        if (sameCol) {
          const int col = boxCells.front().col;
          // Verify digit appears ONLY in this column within the box
          bool appearsOnlyInThisCol = true;
          for (int c = startCol; c < startCol + 3; ++c) {
            if (c == col) continue;
            for (int r = startRow; r < startRow + 3; ++r) {
              if (candidates[r][c].test(digit - 1)) {
                appearsOnlyInThisCol = false;
                break;
              }
            }
            if (!appearsOnlyInThisCol) break;
          }
          if (appearsOnlyInThisCol) {
            // Add pointing cells to show where digit is confined
            for (const HintCell& cell : boxCells) {
              AddUniqueCell(affected, cell.row, cell.col);
            }
            // Add elimination cells
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
        // Verify digit appears ONLY in this row within the box
        // (not in other rows of the same box)
        bool appearsOnlyInThisRow = true;
        for (int r = (row / 3) * 3; r < (row / 3) * 3 + 3; ++r) {
          if (r == row) continue;
          for (int c = boxCol * 3; c < boxCol * 3 + 3; ++c) {
            if (candidates[r][c].test(digit - 1)) {
              appearsOnlyInThisRow = false;
              break;
            }
          }
          if (!appearsOnlyInThisRow) break;
        }
        if (!appearsOnlyInThisRow) continue;

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
        // Verify digit appears ONLY in this column within the box
        // (not in other columns of the same box)
        bool appearsOnlyInThisCol = true;
        for (int c = (col / 3) * 3; c < (col / 3) * 3 + 3; ++c) {
          if (c == col) continue;
          for (int r = boxRow * 3; r < boxRow * 3 + 3; ++r) {
            if (candidates[r][c].test(digit - 1)) {
              appearsOnlyInThisCol = false;
              break;
            }
          }
          if (!appearsOnlyInThisCol) break;
        }
        if (!appearsOnlyInThisCol) continue;

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

      // Only suggest Forcing Chains on cells where the user actually has a bivalue situation.
      // The pivot must have exactly 2 pencil marks (not narrowed down further by the user).
      if (grid[row][col].pencil.count() != 2) {
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
      } else {
        // Cell is solved: clear pencil marks
        next.reset();
      }
      if (cell.pencil != next) {
        cell.pencil = next;
        ++changedCells;
      }
    }
  }
  return changedCells;
}

bool HasStaleOrIncorrectPencilMarks(const Grid& grid) {
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      if (cell.value == 0) {
        // Unsolved cell: check if pencil marks match legal candidates
        const std::bitset<9> legalCandidates = ComputeCandidates(grid, row, col);
        if (cell.pencil != legalCandidates) {
          return true;  // Mismatch found
        }
      }
    }
  }
  return false;  // All pencil marks are consistent
}

std::vector<int> GetMissingCandidates(const Grid& grid, int row, int col) {
  const Cell& cell = grid[row][col];
  std::vector<int> missing;

  if (cell.value != 0) {
    return missing;  // Cell is solved, no missing candidates
  }

  const std::bitset<9> legalCandidates = ComputeCandidates(grid, row, col);

  // Find digits that are legal but not in user's pencil marks
  for (int d = 0; d < 9; ++d) {
    if (legalCandidates.test(d) && !cell.pencil.test(d)) {
      missing.push_back(d + 1);  // Convert 0-indexed to 1-9
    }
  }

  return missing;
}

ValidationWarning ValidateUserCandidates(const Grid& grid) {
  ValidationWarning warning;

  // Check each unsolved cell for erased-but-legal candidates
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      const Cell& cell = grid[row][col];
      if (cell.value == 0) {
        const std::vector<int> missing = GetMissingCandidates(grid, row, col);
        if (!missing.empty()) {
          warning.erasedButLegal.push_back({row, col, missing});
        }
      }
    }
  }

  return warning;
}

int ApplyNakedSingles(Grid& grid) {
  int placements = 0;
  for (int row = 0; row < kGridSize; ++row) {
    for (int col = 0; col < kGridSize; ++col) {
      Cell& cell = grid[row][col];
      if (cell.value != 0) {
        continue;
      }

      // Always use legal candidates. Pencil marks may be stale/incomplete.
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
        // Always use legal candidates. Pencil marks may be stale/incomplete.
        const std::bitset<9> candidates = ComputeCandidates(grid, row, col);
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
        // Always use legal candidates. Pencil marks may be stale/incomplete.
        const std::bitset<9> candidates = ComputeCandidates(grid, row, col);
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

  // Rows
  for (int r = 0; r < kGridSize; ++r) {
    std::vector<std::pair<long unsigned int, std::pair<int, int>>> pairCells;
    for (int c = 0; c < kGridSize; ++c) {
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() == 2) {
        pairCells.push_back({grid[r][c].pencil.to_ulong(), {r, c}});
      }
    }
    for (size_t i = 0; i < pairCells.size(); ++i) {
      for (size_t j = i + 1; j < pairCells.size(); ++j) {
        if (pairCells[i].first == pairCells[j].first) {
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

  // Columns
  for (int c = 0; c < kGridSize; ++c) {
    std::vector<std::pair<long unsigned int, std::pair<int, int>>> pairCells;
    for (int r = 0; r < kGridSize; ++r) {
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() == 2) {
        pairCells.push_back({grid[r][c].pencil.to_ulong(), {r, c}});
      }
    }
    for (size_t i = 0; i < pairCells.size(); ++i) {
      for (size_t j = i + 1; j < pairCells.size(); ++j) {
        if (pairCells[i].first == pairCells[j].first) {
          const auto& pair = pairCells[i].first;
          for (int r = 0; r < kGridSize; ++r) {
            if (r != pairCells[i].second.first && r != pairCells[j].second.first) {
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

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      std::vector<std::pair<long unsigned int, std::pair<int, int>>> pairCells;
      for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
        for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
          if (grid[r][c].value == 0 && grid[r][c].pencil.count() == 2) {
            pairCells.push_back({grid[r][c].pencil.to_ulong(), {r, c}});
          }
        }
      }
      for (size_t i = 0; i < pairCells.size(); ++i) {
        for (size_t j = i + 1; j < pairCells.size(); ++j) {
          if (pairCells[i].first == pairCells[j].first) {
            const auto& pair = pairCells[i].first;
            for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
              for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
                if (r != pairCells[i].second.first || c != pairCells[i].second.second) {
                  if (r != pairCells[j].second.first || c != pairCells[j].second.second) {
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
      }
    }
  }

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

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      for (int d1 = 1; d1 <= 9; ++d1) {
        for (int d2 = d1 + 1; d2 <= 9; ++d2) {
          std::vector<std::pair<int, int>> cellsWithPair;
          for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
            for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
                  grid[r][c].pencil.test(d2 - 1)) {
                cellsWithPair.push_back({r, c});
              }
            }
          }

          if (cellsWithPair.size() == 2) {
            bool d1OnlyInPair = true;
            bool d2OnlyInPair = true;
            for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
              for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
                if (r == cellsWithPair[0].first && c == cellsWithPair[0].second) continue;
                if (r == cellsWithPair[1].first && c == cellsWithPair[1].second) continue;
                if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1)) d1OnlyInPair = false;
                if (grid[r][c].value == 0 && grid[r][c].pencil.test(d2 - 1)) d2OnlyInPair = false;
              }
            }

            if (d1OnlyInPair && d2OnlyInPair) {
              for (const auto& [r, c] : cellsWithPair) {
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

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      std::vector<std::pair<int, int>> tripleCells;
      for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
        for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
          if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 &&
              grid[r][c].pencil.count() <= 3) {
            tripleCells.push_back({r, c});
          }
        }
      }

      for (size_t i = 0; i < tripleCells.size(); ++i) {
        for (size_t j = i + 1; j < tripleCells.size(); ++j) {
          for (size_t k = j + 1; k < tripleCells.size(); ++k) {
            const std::bitset<9> unionCandidates =
                grid[tripleCells[i].first][tripleCells[i].second].pencil |
                grid[tripleCells[j].first][tripleCells[j].second].pencil |
                grid[tripleCells[k].first][tripleCells[k].second].pencil;

            if (unionCandidates.count() == 3) {
              for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
                for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
                  if (r == tripleCells[i].first && c == tripleCells[i].second) continue;
                  if (r == tripleCells[j].first && c == tripleCells[j].second) continue;
                  if (r == tripleCells[k].first && c == tripleCells[k].second) continue;
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

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      for (int d1 = 1; d1 <= 9; ++d1) {
        for (int d2 = d1 + 1; d2 <= 9; ++d2) {
          for (int d3 = d2 + 1; d3 <= 9; ++d3) {
            std::vector<std::pair<int, int>> cellsWithTriple;
            for (int r = boxR * 3; r < boxR * 3 + 3; ++r) {
              for (int c = boxC * 3; c < boxC * 3 + 3; ++c) {
                if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1 - 1) &&
                    grid[r][c].pencil.test(d2 - 1) && grid[r][c].pencil.test(d3 - 1)) {
                  cellsWithTriple.push_back({r, c});
                }
              }
            }

            if (cellsWithTriple.size() == 3) {
              bool allOnlyInTriple = true;
              for (int r = boxR * 3; r < boxR * 3 + 3 && allOnlyInTriple; ++r) {
                for (int c = boxC * 3; c < boxC * 3 + 3 && allOnlyInTriple; ++c) {
                  if (r == cellsWithTriple[0].first && c == cellsWithTriple[0].second) continue;
                  if (r == cellsWithTriple[1].first && c == cellsWithTriple[1].second) continue;
                  if (r == cellsWithTriple[2].first && c == cellsWithTriple[2].second) continue;
                  if (grid[r][c].value == 0 &&
                      (grid[r][c].pencil.test(d1 - 1) || grid[r][c].pencil.test(d2 - 1) ||
                       grid[r][c].pencil.test(d3 - 1))) {
                    allOnlyInTriple = false;
                  }
                }
              }

              if (allOnlyInTriple) {
                for (const auto& [r, c] : cellsWithTriple) {
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
  }

  return madeChanges ? 1 : 0;
}

int ApplyNakedQuads(Grid& grid) {
  bool madeChanges = false;

  // Rows
  for (int r = 0; r < kGridSize; ++r) {
    std::vector<int> qc;
    for (int c = 0; c < kGridSize; ++c)
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 && grid[r][c].pencil.count() <= 4)
        qc.push_back(c);
    for (size_t i = 0; i < qc.size(); ++i)
      for (size_t j = i+1; j < qc.size(); ++j)
        for (size_t k = j+1; k < qc.size(); ++k)
          for (size_t l = k+1; l < qc.size(); ++l) {
            const std::bitset<9> un = grid[r][qc[i]].pencil | grid[r][qc[j]].pencil |
                                      grid[r][qc[k]].pencil | grid[r][qc[l]].pencil;
            if (un.count() != 4) continue;
            for (int c = 0; c < kGridSize; ++c) {
              if (c == qc[i] || c == qc[j] || c == qc[k] || c == qc[l]) continue;
              for (int d = 0; d < 9; ++d)
                if (un.test(d) && grid[r][c].pencil.test(d)) { grid[r][c].pencil.reset(d); madeChanges = true; }
            }
          }
  }

  // Columns
  for (int c = 0; c < kGridSize; ++c) {
    std::vector<int> qc;
    for (int r = 0; r < kGridSize; ++r)
      if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 && grid[r][c].pencil.count() <= 4)
        qc.push_back(r);
    for (size_t i = 0; i < qc.size(); ++i)
      for (size_t j = i+1; j < qc.size(); ++j)
        for (size_t k = j+1; k < qc.size(); ++k)
          for (size_t l = k+1; l < qc.size(); ++l) {
            const std::bitset<9> un = grid[qc[i]][c].pencil | grid[qc[j]][c].pencil |
                                      grid[qc[k]][c].pencil | grid[qc[l]][c].pencil;
            if (un.count() != 4) continue;
            for (int r = 0; r < kGridSize; ++r) {
              if (r == qc[i] || r == qc[j] || r == qc[k] || r == qc[l]) continue;
              for (int d = 0; d < 9; ++d)
                if (un.test(d) && grid[r][c].pencil.test(d)) { grid[r][c].pencil.reset(d); madeChanges = true; }
            }
          }
  }

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      std::vector<std::pair<int,int>> qc;
      for (int r = boxR*3; r < boxR*3+3; ++r)
        for (int c = boxC*3; c < boxC*3+3; ++c)
          if (grid[r][c].value == 0 && grid[r][c].pencil.count() >= 2 && grid[r][c].pencil.count() <= 4)
            qc.push_back({r, c});
      for (size_t i = 0; i < qc.size(); ++i)
        for (size_t j = i+1; j < qc.size(); ++j)
          for (size_t k = j+1; k < qc.size(); ++k)
            for (size_t l = k+1; l < qc.size(); ++l) {
              const std::bitset<9> un = grid[qc[i].first][qc[i].second].pencil |
                                        grid[qc[j].first][qc[j].second].pencil |
                                        grid[qc[k].first][qc[k].second].pencil |
                                        grid[qc[l].first][qc[l].second].pencil;
              if (un.count() != 4) continue;
              for (int r = boxR*3; r < boxR*3+3; ++r) {
                for (int c = boxC*3; c < boxC*3+3; ++c) {
                  if ((r==qc[i].first&&c==qc[i].second)||(r==qc[j].first&&c==qc[j].second)||
                      (r==qc[k].first&&c==qc[k].second)||(r==qc[l].first&&c==qc[l].second)) continue;
                  for (int d = 0; d < 9; ++d)
                    if (un.test(d) && grid[r][c].pencil.test(d)) { grid[r][c].pencil.reset(d); madeChanges = true; }
                }
              }
            }
    }
  }

  return madeChanges ? 1 : 0;
}

int ApplyHiddenQuads(Grid& grid) {
  bool madeChanges = false;

  // Rows
  for (int r = 0; r < kGridSize; ++r) {
    for (int d1 = 1; d1 <= 9; ++d1)
      for (int d2 = d1+1; d2 <= 9; ++d2)
        for (int d3 = d2+1; d3 <= 9; ++d3)
          for (int d4 = d3+1; d4 <= 9; ++d4) {
            std::vector<int> qc;
            for (int c = 0; c < kGridSize; ++c)
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1-1) && grid[r][c].pencil.test(d2-1) &&
                  grid[r][c].pencil.test(d3-1) && grid[r][c].pencil.test(d4-1))
                qc.push_back(c);
            if (qc.size() != 4) continue;
            bool only = true;
            for (int c = 0; c < kGridSize && only; ++c) {
              if (std::find(qc.begin(), qc.end(), c) != qc.end()) continue;
              if (grid[r][c].value == 0 && (grid[r][c].pencil.test(d1-1) || grid[r][c].pencil.test(d2-1) ||
                  grid[r][c].pencil.test(d3-1) || grid[r][c].pencil.test(d4-1))) only = false;
            }
            if (!only) continue;
            for (int c : qc)
              for (int d = 1; d <= 9; ++d)
                if (d != d1 && d != d2 && d != d3 && d != d4 && grid[r][c].pencil.test(d-1)) {
                  grid[r][c].pencil.reset(d-1); madeChanges = true;
                }
          }
  }

  // Columns
  for (int c = 0; c < kGridSize; ++c) {
    for (int d1 = 1; d1 <= 9; ++d1)
      for (int d2 = d1+1; d2 <= 9; ++d2)
        for (int d3 = d2+1; d3 <= 9; ++d3)
          for (int d4 = d3+1; d4 <= 9; ++d4) {
            std::vector<int> qc;
            for (int r = 0; r < kGridSize; ++r)
              if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1-1) && grid[r][c].pencil.test(d2-1) &&
                  grid[r][c].pencil.test(d3-1) && grid[r][c].pencil.test(d4-1))
                qc.push_back(r);
            if (qc.size() != 4) continue;
            bool only = true;
            for (int r = 0; r < kGridSize && only; ++r) {
              if (std::find(qc.begin(), qc.end(), r) != qc.end()) continue;
              if (grid[r][c].value == 0 && (grid[r][c].pencil.test(d1-1) || grid[r][c].pencil.test(d2-1) ||
                  grid[r][c].pencil.test(d3-1) || grid[r][c].pencil.test(d4-1))) only = false;
            }
            if (!only) continue;
            for (int r : qc)
              for (int d = 1; d <= 9; ++d)
                if (d != d1 && d != d2 && d != d3 && d != d4 && grid[r][c].pencil.test(d-1)) {
                  grid[r][c].pencil.reset(d-1); madeChanges = true;
                }
          }
  }

  // Boxes
  for (int boxR = 0; boxR < 3; ++boxR) {
    for (int boxC = 0; boxC < 3; ++boxC) {
      for (int d1 = 1; d1 <= 9; ++d1)
        for (int d2 = d1+1; d2 <= 9; ++d2)
          for (int d3 = d2+1; d3 <= 9; ++d3)
            for (int d4 = d3+1; d4 <= 9; ++d4) {
              std::vector<std::pair<int,int>> qc;
              for (int r = boxR*3; r < boxR*3+3; ++r)
                for (int c = boxC*3; c < boxC*3+3; ++c)
                  if (grid[r][c].value == 0 && grid[r][c].pencil.test(d1-1) && grid[r][c].pencil.test(d2-1) &&
                      grid[r][c].pencil.test(d3-1) && grid[r][c].pencil.test(d4-1))
                    qc.push_back({r, c});
              if (qc.size() != 4) continue;
              bool only = true;
              for (int r = boxR*3; r < boxR*3+3 && only; ++r) {
                for (int c = boxC*3; c < boxC*3+3 && only; ++c) {
                  if (ContainsCell({{qc[0].first,qc[0].second},{qc[1].first,qc[1].second},
                                    {qc[2].first,qc[2].second},{qc[3].first,qc[3].second}}, r, c)) continue;
                  if (grid[r][c].value == 0 && (grid[r][c].pencil.test(d1-1) || grid[r][c].pencil.test(d2-1) ||
                      grid[r][c].pencil.test(d3-1) || grid[r][c].pencil.test(d4-1))) only = false;
                }
              }
              if (!only) continue;
              for (const auto& [r, c] : qc)
                for (int d = 1; d <= 9; ++d)
                  if (d != d1 && d != d2 && d != d3 && d != d4 && grid[r][c].pencil.test(d-1)) {
                    grid[r][c].pencil.reset(d-1); madeChanges = true;
                  }
            }
    }
  }

  return madeChanges ? 1 : 0;
}

int ApplyBlockBlockInteraction(Grid& grid) {
  bool madeChanges = false;

  // Band interactions: digit confined to same rows in two boxes → eliminate from third box
  for (int band = 0; band < 3; ++band) {
    const int startRow = band * 3;
    for (int digit = 1; digit <= 9; ++digit) {
      std::array<std::bitset<3>, 3> rowMasks{};
      for (int boxCol = 0; boxCol < 3; ++boxCol) {
        const int startCol = boxCol * 3;
        for (int r = 0; r < 3; ++r)
          for (int c = 0; c < 3; ++c)
            if (grid[startRow+r][startCol+c].pencil.test(digit-1)) rowMasks[boxCol].set(r);
      }
      for (int boxA = 0; boxA < 2; ++boxA) {
        for (int boxB = boxA+1; boxB < 3; ++boxB) {
          const std::bitset<3> shared = rowMasks[boxA] & rowMasks[boxB];
          if (shared.count() != 2 || rowMasks[boxA] != shared || rowMasks[boxB] != shared) continue;
          const int boxC = 3 - boxA - boxB;
          const int startColC = boxC * 3;
          for (int rr = 0; rr < 3; ++rr) {
            if (!shared.test(rr)) continue;
            const int row = startRow + rr;
            for (int cc = 0; cc < 3; ++cc)
              if (grid[row][startColC+cc].pencil.test(digit-1)) {
                grid[row][startColC+cc].pencil.reset(digit-1); madeChanges = true;
              }
          }
        }
      }
    }
  }

  // Stack interactions: digit confined to same cols in two boxes → eliminate from third box
  for (int stack = 0; stack < 3; ++stack) {
    const int startCol = stack * 3;
    for (int digit = 1; digit <= 9; ++digit) {
      std::array<std::bitset<3>, 3> colMasks{};
      for (int boxRow = 0; boxRow < 3; ++boxRow) {
        const int sRow = boxRow * 3;
        for (int r = 0; r < 3; ++r)
          for (int c = 0; c < 3; ++c)
            if (grid[sRow+r][startCol+c].pencil.test(digit-1)) colMasks[boxRow].set(c);
      }
      for (int boxA = 0; boxA < 2; ++boxA) {
        for (int boxB = boxA+1; boxB < 3; ++boxB) {
          const std::bitset<3> shared = colMasks[boxA] & colMasks[boxB];
          if (shared.count() != 2 || colMasks[boxA] != shared || colMasks[boxB] != shared) continue;
          const int boxC = 3 - boxA - boxB;
          const int startRowC = boxC * 3;
          for (int cc = 0; cc < 3; ++cc) {
            if (!shared.test(cc)) continue;
            const int col = startCol + cc;
            for (int rr = 0; rr < 3; ++rr)
              if (grid[startRowC+rr][col].pencil.test(digit-1)) {
                grid[startRowC+rr][col].pencil.reset(digit-1); madeChanges = true;
              }
          }
        }
      }
    }
  }

  return madeChanges ? 1 : 0;
}

// Generic fish (X-Wing = size 2, Swordfish = 3, Jellyfish = 4).
static int ApplyFishImpl(Grid& grid, int size) {
  bool madeChanges = false;
  for (int digit = 1; digit <= 9; ++digit) {
    // Row-based
    std::vector<int> baseRows;
    for (int row = 0; row < kGridSize; ++row) {
      int count = 0;
      for (int col = 0; col < kGridSize; ++col)
        if (grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) ++count;
      if (count >= 2 && count <= size) baseRows.push_back(row);
    }
    std::vector<int> choice;
    ForEachCombination(baseRows, size, 0, choice, [&](const std::vector<int>& rows) -> bool {
      std::bitset<9> colsMask;
      for (int row : rows)
        for (int col = 0; col < kGridSize; ++col)
          if (grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) colsMask.set(col);
      if (static_cast<int>(colsMask.count()) != size) return false;
      for (int row = 0; row < kGridSize; ++row) {
        if (std::find(rows.begin(), rows.end(), row) != rows.end()) continue;
        for (int col = 0; col < kGridSize; ++col)
          if (colsMask.test(col) && grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) {
            grid[row][col].pencil.reset(digit-1); madeChanges = true;
          }
      }
      return false;
    });

    // Col-based
    std::vector<int> baseCols;
    for (int col = 0; col < kGridSize; ++col) {
      int count = 0;
      for (int row = 0; row < kGridSize; ++row)
        if (grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) ++count;
      if (count >= 2 && count <= size) baseCols.push_back(col);
    }
    choice.clear();
    ForEachCombination(baseCols, size, 0, choice, [&](const std::vector<int>& cols) -> bool {
      std::bitset<9> rowsMask;
      for (int col : cols)
        for (int row = 0; row < kGridSize; ++row)
          if (grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) rowsMask.set(row);
      if (static_cast<int>(rowsMask.count()) != size) return false;
      for (int col = 0; col < kGridSize; ++col) {
        if (std::find(cols.begin(), cols.end(), col) != cols.end()) continue;
        for (int row = 0; row < kGridSize; ++row)
          if (rowsMask.test(row) && grid[row][col].value == 0 && grid[row][col].pencil.test(digit-1)) {
            grid[row][col].pencil.reset(digit-1); madeChanges = true;
          }
      }
      return false;
    });
  }
  return madeChanges ? 1 : 0;
}

int ApplyXWing(Grid& grid)    { return ApplyFishImpl(grid, 2); }
int ApplySwordfish(Grid& grid) { return ApplyFishImpl(grid, 3); }
int ApplyJellyfish(Grid& grid) { return ApplyFishImpl(grid, 4); }

int ApplyUniqueRectangle(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectUniqueRectangleType1(candidates);
  if (!hint.IsValid() || hint.affectedCells.size() < 5 || hint.involvedDigits.empty()) return 0;
  // affectedCells = {a, b, c, d, extraCell}; the 5th entry is the cell with extra candidates.
  const HintCell extraCell = hint.affectedCells[4];
  int changes = 0;
  for (int digit : hint.involvedDigits)
    if (grid[extraCell.row][extraCell.col].value == 0 && grid[extraCell.row][extraCell.col].pencil.test(digit-1)) {
      grid[extraCell.row][extraCell.col].pencil.reset(digit-1); ++changes;
    }
  return changes > 0 ? 1 : 0;
}

int ApplyYWing(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectXYWing(candidates);
  if (!hint.IsValid() || hint.affectedCells.size() < 3) return 0;
  // affectedCells = {pivot, wingA, wingB, ...targets}; z-digit = wingA ∩ wingB.
  const HintCell wingA = hint.affectedCells[1];
  const HintCell wingB = hint.affectedCells[2];
  const std::bitset<9> zMask = candidates[wingA.row][wingA.col] & candidates[wingB.row][wingB.col];
  int changes = 0;
  for (size_t i = 3; i < hint.affectedCells.size(); ++i) {
    const HintCell& cell = hint.affectedCells[i];
    for (int d = 0; d < 9; ++d)
      if (zMask.test(d) && grid[cell.row][cell.col].value == 0 && grid[cell.row][cell.col].pencil.test(d)) {
        grid[cell.row][cell.col].pencil.reset(d); ++changes;
      }
  }
  return changes > 0 ? 1 : 0;
}

int ApplyXYZWing(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectXYZWing(candidates);
  if (!hint.IsValid() || hint.affectedCells.size() < 3) return 0;
  // affectedCells = {pivot, wingA, wingB, ...targets}; z-digit = wingA ∩ wingB.
  const HintCell wingA = hint.affectedCells[1];
  const HintCell wingB = hint.affectedCells[2];
  const std::bitset<9> zMask = candidates[wingA.row][wingA.col] & candidates[wingB.row][wingB.col];
  int changes = 0;
  for (size_t i = 3; i < hint.affectedCells.size(); ++i) {
    const HintCell& cell = hint.affectedCells[i];
    for (int d = 0; d < 9; ++d)
      if (zMask.test(d) && grid[cell.row][cell.col].value == 0 && grid[cell.row][cell.col].pencil.test(d)) {
        grid[cell.row][cell.col].pencil.reset(d); ++changes;
      }
  }
  return changes > 0 ? 1 : 0;
}

int ApplySimpleColoring(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectSimpleColoring(candidates);
  if (!hint.IsValid() || hint.affectedCells.empty() || hint.involvedDigits.empty()) return 0;
  // The target cell (sees both color groups) is appended last in affectedCells.
  const HintCell target = hint.affectedCells.back();
  const int digit = hint.involvedDigits[0];
  if (grid[target.row][target.col].value == 0 && grid[target.row][target.col].pencil.test(digit-1)) {
    grid[target.row][target.col].pencil.reset(digit-1);
    return 1;
  }
  return 0;
}

int ApplyXYChain(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectXYChain(candidates);
  if (!hint.IsValid()) return 0;
  // chainCells are the source links; non-chain cells in affectedCells are elimination targets.
  int changes = 0;
  for (const HintCell& cell : hint.affectedCells) {
    if (ContainsCell(hint.chainCells, cell.row, cell.col)) continue;
    for (int digit : hint.involvedDigits)
      if (grid[cell.row][cell.col].value == 0 && grid[cell.row][cell.col].pencil.test(digit-1)) {
        grid[cell.row][cell.col].pencil.reset(digit-1); ++changes;
      }
  }
  return changes > 0 ? 1 : 0;
}

int ApplyForcingChains(Grid& grid) {
  const CandidateGrid candidates = BuildCandidateGrid(grid);
  const Hint hint = DetectForcingChains(grid, candidates);
  if (!hint.IsValid() || hint.affectedCells.size() < 2 || hint.involvedDigits.empty()) return 0;

  const HintCell pivot  = hint.affectedCells[0];
  const HintCell target = hint.affectedCells[1];
  const int digit = hint.involvedDigits[0];
  if (grid[target.row][target.col].value != 0) return 0;

  std::vector<int> pivotDigits;
  for (int d = 0; d < 9; ++d)
    if (candidates[pivot.row][pivot.col].test(d)) pivotDigits.push_back(d+1);
  if (pivotDigits.size() != 2) return 0;

  const ForcingBranchResult left  = SimulateForcingBranch(grid, pivot.row, pivot.col, pivotDigits[0]);
  const ForcingBranchResult right = SimulateForcingBranch(grid, pivot.row, pivot.col, pivotDigits[1]);

  // Common placement: both branches force the same value at target.
  if (!left.contradiction && !right.contradiction &&
      left.grid[target.row][target.col].value == digit &&
      right.grid[target.row][target.col].value == digit) {
    grid[target.row][target.col].value = digit;
    grid[target.row][target.col].pencil.reset();
    RemoveDigitFromPeerPencils(grid, target.row, target.col, digit);
    return 1;
  }

  // Common elimination: both branches eliminate digit from target.
  if (!left.candidates[target.row][target.col].test(digit-1) &&
      !right.candidates[target.row][target.col].test(digit-1) &&
      grid[target.row][target.col].pencil.test(digit-1)) {
    grid[target.row][target.col].pencil.reset(digit-1);
    return 1;
  }

  return 0;
}

int ApplyHint(Grid& grid, const Hint& hint) {
  const std::string& name = hint.techniqueName;
  if (name == "Naked Single")                return ApplyNakedSingles(grid);
  if (name == "Hidden Single")               return ApplyHiddenSingles(grid);
  if (name == "Pointing Pair/Triple")        return ApplyPointingPairs(grid);
  if (name == "Box/Line Reduction")          return ApplyBoxLineReduction(grid);
  if (name == "Naked Pair")                  return ApplyNakedPairs(grid);
  if (name == "Hidden Pair")                 return ApplyHiddenPairs(grid);
  if (name == "Naked Triple")                return ApplyNakedTriples(grid);
  if (name == "Hidden Triple")               return ApplyHiddenTriples(grid);
  if (name == "Naked Quad")                  return ApplyNakedQuads(grid);
  if (name == "Hidden Quad")                 return ApplyHiddenQuads(grid);
  if (name == "Block/Block Interaction")     return ApplyBlockBlockInteraction(grid);
  if (name == "X-Wing")                      return ApplyXWing(grid);
  if (name == "Unique Rectangle")            return ApplyUniqueRectangle(grid);
  if (name == "Y-Wing")                      return ApplyYWing(grid);
  if (name == "Simple Colouring")            return ApplySimpleColoring(grid);
  if (name == "Swordfish")                   return ApplySwordfish(grid);
  if (name == "XYZ-Wing")                    return ApplyXYZWing(grid);
  if (name == "XY-Chain")                    return ApplyXYChain(grid);
  if (name == "Jellyfish")                   return ApplyJellyfish(grid);
  if (name == "Forcing Chains")              return ApplyForcingChains(grid);
  return 0;
}

Hint GenerateHint(const Grid& grid) {
  // Use user's pencil marks as source of truth; respect their eliminations
  const CandidateGrid candidates = BuildCandidateGrid(grid);

  // Helper: returns the hint only if it would actually change the pencil state.
  // Detectors use full legal candidates; pencil marks may have already captured
  // the same eliminations, so we test-apply before committing.
  auto ActionableHint = [&](Hint h) -> Hint {
    if (!h.IsValid()) return {};
    Grid testGrid = grid;
    if (ApplyHint(testGrid, h) == 0) return {};
    return h;
  };

  // Ordered from easier/common techniques to heavier analysis.
  // For basic techniques, return hints directly from detectors since they're based on
  // rebuild legal candidates (BuildFullCandidateGrid). Pencil marks may not be in sync.
  if (Hint h = DetectNakedSingles(candidates); h.IsValid()) return h;
  if (Hint h = DetectHiddenSingles(candidates); h.IsValid()) return h;

  const std::array<Hint (*)(const CandidateGrid&), 8> detectors = {
      DetectPointingPairs,
      DetectBoxLineReduction,
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 2, "Naked Pair"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 2, "Hidden Pair"); },
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 3, "Naked Triple"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 3, "Hidden Triple"); },
      [](const CandidateGrid& c) { return DetectNakedSubset(c, 4, "Naked Quad"); },
      [](const CandidateGrid& c) { return DetectHiddenSubset(c, 4, "Hidden Quad"); },
  };

  for (const auto& detect : detectors) {
    if (Hint h = ActionableHint(detect(candidates)); h.IsValid()) return h;
  }

  if (Hint h = ActionableHint(DetectBlockBlockInteraction(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectFish(candidates, 2, "X-Wing")); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectUniqueRectangleType1(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectXYWing(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectSimpleColoring(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectFish(candidates, 3, "Swordfish")); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectXYZWing(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectXYChain(candidates)); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectFish(candidates, 4, "Jellyfish")); h.IsValid()) return h;
  if (Hint h = ActionableHint(DetectForcingChains(grid, candidates)); h.IsValid()) return h;

  Hint emptyHint;
  emptyHint.techniqueName = "No supported Sadman hint available";
  // Validate user's pencil marks when no solving hint is found
  emptyHint.validation = ValidateUserCandidates(grid);
  return emptyHint;
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
    while (ApplyNakedSingles(grid) > 0) {
      changed = true;
    }

    // Apply hidden singles until stable
    while (ApplyHiddenSingles(grid) > 0) {
      changed = true;
    }

    // Apply pointing pairs until stable
    while (ApplyPointingPairs(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply box/line reduction until stable
    while (ApplyBoxLineReduction(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply naked pairs until stable
    while (ApplyNakedPairs(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply hidden pairs until stable
    while (ApplyHiddenPairs(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply naked triples until stable
    while (ApplyNakedTriples(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply hidden triples until stable
    while (ApplyHiddenTriples(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply naked quads until stable
    while (ApplyNakedQuads(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply hidden quads until stable
    while (ApplyHiddenQuads(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply block/block interaction until stable
    while (ApplyBlockBlockInteraction(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply X-Wing until stable
    while (ApplyXWing(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Unique Rectangle until stable
    while (ApplyUniqueRectangle(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Y-Wing until stable
    while (ApplyYWing(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Simple Colouring until stable
    while (ApplySimpleColoring(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Swordfish until stable
    while (ApplySwordfish(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply XYZ-Wing until stable
    while (ApplyXYZWing(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply XY-Chain until stable
    while (ApplyXYChain(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Jellyfish until stable
    while (ApplyJellyfish(grid) > 0) {
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Apply Forcing Chains until stable
    while (ApplyForcingChains(grid) > 0) {
      while (ApplyNakedSingles(grid) > 0 || ApplyHiddenSingles(grid) > 0) {
        // Keep propagating
      }
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
    case SolvingTechnique::kNakedQuad:
      return 50;
    case SolvingTechnique::kHiddenQuad:
      return 60;
    case SolvingTechnique::kBlockBlockInteraction:
      return 25;
    case SolvingTechnique::kXWing:
      return 80;
    case SolvingTechnique::kUniqueRectangle:
      return 70;
    case SolvingTechnique::kYWing:
      return 100;
    case SolvingTechnique::kSimpleColouring:
      return 120;
    case SolvingTechnique::kSwordfish:
      return 140;
    case SolvingTechnique::kXYZWing:
      return 150;
    case SolvingTechnique::kXYChain:
      return 200;
    case SolvingTechnique::kJellyfish:
      return 200;
    case SolvingTechnique::kForcingChains:
      return 300;
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
    // Use same algorithm as SolveComprehensive: exhaust each technique before moving on
    bool changed = false;

    // Naked Singles — apply until stable
    while (ApplyNakedSingles(grid) > 0) {
      score.nakedSingles += GetTechniqueScore(SolvingTechnique::kNakedSingle);
      changed = true;
    }

    // Hidden Singles — apply until stable
    while (ApplyHiddenSingles(grid) > 0) {
      score.hiddenSingles += GetTechniqueScore(SolvingTechnique::kHiddenSingle);
      changed = true;
    }

    // Pointing Pairs — apply until stable
    while (ApplyPointingPairs(grid) > 0) {
      score.pointingPairs += GetTechniqueScore(SolvingTechnique::kPointingPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Box/Line Reduction — apply until stable
    while (ApplyBoxLineReduction(grid) > 0) {
      score.boxLineReductions += GetTechniqueScore(SolvingTechnique::kBoxLineReduction);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Naked Pairs — apply until stable
    while (ApplyNakedPairs(grid) > 0) {
      score.nakedPairs += GetTechniqueScore(SolvingTechnique::kNakedPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Hidden Pairs — apply until stable
    while (ApplyHiddenPairs(grid) > 0) {
      score.hiddenPairs += GetTechniqueScore(SolvingTechnique::kHiddenPair);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Naked Triples — apply until stable
    while (ApplyNakedTriples(grid) > 0) {
      score.nakedTriples += GetTechniqueScore(SolvingTechnique::kNakedTriple);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Hidden Triples — apply until stable
    while (ApplyHiddenTriples(grid) > 0) {
      score.hiddenTriples += GetTechniqueScore(SolvingTechnique::kHiddenTriple);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Naked Quads — apply until stable
    while (ApplyNakedQuads(grid) > 0) {
      score.nakedQuads += GetTechniqueScore(SolvingTechnique::kNakedQuad);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Hidden Quads — apply until stable
    while (ApplyHiddenQuads(grid) > 0) {
      score.hiddenQuads += GetTechniqueScore(SolvingTechnique::kHiddenQuad);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Block/Block Interaction — apply until stable
    while (ApplyBlockBlockInteraction(grid) > 0) {
      score.blockBlockInteractions += GetTechniqueScore(SolvingTechnique::kBlockBlockInteraction);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // X-Wing — apply until stable
    while (ApplyXWing(grid) > 0) {
      score.xWings += GetTechniqueScore(SolvingTechnique::kXWing);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Unique Rectangle — apply until stable
    while (ApplyUniqueRectangle(grid) > 0) {
      score.uniqueRectangles += GetTechniqueScore(SolvingTechnique::kUniqueRectangle);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Y-Wing — apply until stable
    while (ApplyYWing(grid) > 0) {
      score.yWings += GetTechniqueScore(SolvingTechnique::kYWing);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Simple Colouring — apply until stable
    while (ApplySimpleColoring(grid) > 0) {
      score.simpleColourings += GetTechniqueScore(SolvingTechnique::kSimpleColouring);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Swordfish — apply until stable
    while (ApplySwordfish(grid) > 0) {
      score.swordfishes += GetTechniqueScore(SolvingTechnique::kSwordfish);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // XYZ-Wing — apply until stable
    while (ApplyXYZWing(grid) > 0) {
      score.xyzWings += GetTechniqueScore(SolvingTechnique::kXYZWing);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // XY-Chain — apply until stable
    while (ApplyXYChain(grid) > 0) {
      score.xyChains += GetTechniqueScore(SolvingTechnique::kXYChain);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Jellyfish — apply until stable
    while (ApplyJellyfish(grid) > 0) {
      score.jellyfishes += GetTechniqueScore(SolvingTechnique::kJellyfish);
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    // Forcing Chains — apply until stable
    while (ApplyForcingChains(grid) > 0) {
      score.forcingChains += GetTechniqueScore(SolvingTechnique::kForcingChains);
      while (ApplyNakedSingles(grid) > 0 || ApplyHiddenSingles(grid) > 0) {
        // Keep propagating
      }
      changed = true;
    }

    if (!changed) break;  // No technique made progress
  }

  score.totalScore = score.nakedSingles + score.hiddenSingles + score.pointingPairs +
                     score.boxLineReductions + score.nakedPairs + score.hiddenPairs +
                     score.nakedTriples + score.hiddenTriples + score.nakedQuads +
                     score.hiddenQuads + score.blockBlockInteractions + score.xWings +
                     score.uniqueRectangles + score.yWings + score.simpleColourings +
                     score.swordfishes + score.xyzWings + score.xyChains +
                     score.jellyfishes + score.forcingChains;
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
