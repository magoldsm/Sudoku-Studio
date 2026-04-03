#pragma once

#include "hints_candidates.h"
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
  kNakedQuad = 9,
  kHiddenQuad = 10,
  kBlockBlockInteraction = 11,
  kXWing = 12,
  kUniqueRectangle = 13,
  kYWing = 14,
  kSimpleColouring = 15,
  kSwordfish = 16,
  kXYZWing = 17,
  kXYChain = 18,
  kJellyfish = 19,
  kForcingChains = 20,
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

int GetTechniqueScore(SolvingTechnique technique);

// Returns the next supported hint in difficulty order.
Hint GenerateHint(const Grid& grid);
// Applies the technique named in hint.techniqueName; returns number of changes.
int ApplyHint(Grid& grid, const Hint& hint);
// Rebuilds legal pencil marks for unsolved cells; returns changed cell count.
int ApplyAutoPencil(Grid& grid);
// Checks if user's pencil marks differ from legal candidates; reports mismatch if found.
bool HasStaleOrIncorrectPencilMarks(const Grid& grid);
// Computes missing legal candidates for a specific cell (what user erased that's still legal)
std::vector<int> GetMissingCandidates(const Grid& grid, int row, int col);
// Validates user's pencil marks against legal candidates; returns erased-but-legal candidates
ValidationWarning ValidateUserCandidates(const Grid& grid);
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
int ApplyNakedQuads(Grid& grid);
int ApplyHiddenQuads(Grid& grid);
int ApplyBlockBlockInteraction(Grid& grid);
int ApplyXWing(Grid& grid);
int ApplyUniqueRectangle(Grid& grid);
int ApplyYWing(Grid& grid);
int ApplySimpleColoring(Grid& grid);
int ApplySwordfish(Grid& grid);
int ApplyXYZWing(Grid& grid);
int ApplyXYChain(Grid& grid);
int ApplyJellyfish(Grid& grid);
int ApplyForcingChains(Grid& grid);
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
