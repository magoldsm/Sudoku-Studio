#pragma once

#include "sudoku_core.h"

namespace sudoku {

// Returns the next supported hint in difficulty order.
Hint GenerateHint(const Grid& grid);
// Rebuilds legal pencil marks for unsolved cells; returns changed cell count.
int ApplyAutoPencil(Grid& grid);
// Applies one pass of placements; returns number of placed values.
int ApplyNakedSingles(Grid& grid);
int ApplyHiddenSingles(Grid& grid);
// Repeats single-pass placement until stable; returns total placements.
int AutoSolveNakedSingles(Grid& grid);
int AutoSolveHiddenSingles(Grid& grid);
// Headless consistency checks for detectors and generated hints.
int RunHintSelfChecks();

}  // namespace sudoku
