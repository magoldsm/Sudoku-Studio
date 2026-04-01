#pragma once

#include "sudoku_core.h"

namespace sudoku {

// Solving techniques with point values
enum class SolvingTechnique {
  kNakedSingle = 1,
  kHiddenSingle = 2,
  kPointingPair = 3,
  kBoxLineReduction = 4,
  kNakedPair = 5,
  kHiddenPair = 6,
  kNakedTriple = 7,
  kHiddenTriple = 8,
};

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
};

int GetTechniqueScore(SolvingTechnique technique);

// Utility for working with hint cells.
bool ContainsCell(const std::vector<HintCell>& cells, int row, int col);

// Returns the next supported hint in difficulty order.
Hint GenerateHint(const Grid& grid);
// Rebuilds legal pencil marks for unsolved cells; returns changed cell count.
int ApplyAutoPencil(Grid& grid);
// Applies one pass of placements; returns number of placed values.
int ApplyNakedSingles(Grid& grid);
int ApplyHiddenSingles(Grid& grid);
// Intermediate solving techniques
int ApplyPointingPairs(Grid& grid);
int ApplyBoxLineReduction(Grid& grid);
int ApplyNakedPairs(Grid& grid);
int ApplyHiddenPairs(Grid& grid);
int ApplyNakedTriples(Grid& grid);
int ApplyHiddenTriples(Grid& grid);
// Repeats single-pass placement until stable; returns total placements.
int AutoSolveNakedSingles(Grid& grid);
int AutoSolveHiddenSingles(Grid& grid);
// Comprehensive solver using all available solving techniques
bool SolveComprehensive(Puzzle& puzzle);
// Measures puzzle difficulty (1-8) based on techniques required
// Returns difficulty level where puzzle gets stuck
int MeasurePuzzleDifficulty(const Puzzle& puzzle);
// Solves puzzle and returns total difficulty score
// Score is sum of technique points across all solving steps
int SolveAndScore(const Puzzle& puzzle);
// Solves puzzle and returns detailed scoring breakdown
PuzzleScore SolveAndScoreDetailed(const Puzzle& puzzle);
// Headless consistency checks for detectors and generated hints.
int RunHintSelfChecks();

}  // namespace sudoku
