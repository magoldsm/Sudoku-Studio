#pragma once

#include "sudoku_core.h"

#include <array>
#include <bitset>
#include <vector>

namespace sudoku {

// Candidate grid: for each cell, which digits are legal candidates
using CandidateGrid = std::array<std::array<std::bitset<9>, kGridSize>, kGridSize>;

// A unit is a row, column, or box with 9 cells
struct UnitCells {
  std::array<HintCell, 9> cells;
};

// Builds a candidate grid respecting the grid's current pencil marks
CandidateGrid BuildCandidateGrid(const Grid& grid);

// Builds a candidate grid with full legal candidates, ignoring pencil marks
// Used by GenerateHint to find hints that pencil marks might hide
CandidateGrid BuildFullCandidateGrid(const Grid& grid);

// Builds all 27 units: 9 rows, 9 columns, 9 boxes
std::vector<UnitCells> BuildAllUnits();

// Check if a cell is in the given list
bool ContainsCell(const std::vector<HintCell>& cells, int row, int col);

// Utility: add cell to vector if not already present
void AddUniqueCell(std::vector<HintCell>& cells, int row, int col);

// Utility: check if target cell shares a unit with both left and right
bool SeesBoth(const HintCell& target, const HintCell& left, const HintCell& right);

// Utility: extract digit values from a bitset
std::vector<int> ExtractDigits(const std::bitset<9>& mask);

// Utility: iterate over all k-combinations of values
template <typename Func>
bool ForEachCombination(const std::vector<int>& values,
                        int choose,
                        int start,
                        std::vector<int>& current,
                        Func&& func);

}  // namespace sudoku

// Template implementation must be in header
namespace sudoku {

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

}  // namespace sudoku
