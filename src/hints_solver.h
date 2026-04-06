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

int GetTechniqueScore(SolvingTechnique technique);

// Returns the next supported hint in difficulty order.
Hint GenerateHint(const Grid& grid);
// Applies the technique named in hint.techniqueName; returns number of changes.
int ApplyHint(Grid& grid, const Hint& hint);
// Applies hint using the candidate grid it was detected with (ensures consistency)
int ApplyHintWithCandidates(Grid& grid, const Hint& hint, const CandidateGrid& candidates);
// Rebuilds legal pencil marks for unsolved cells; returns changed cell count.
int ApplyAutoPencil(Grid& grid);
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
// Applies all 20 techniques in priority order for one pass.
// Returns number of technique firings (0 = no progress this pass).
// If score is non-null, accumulates difficulty score per technique.
// If techniques is non-null, records name of each technique that fired.
int RunOnePass(Grid& grid,
               PuzzleScore* score = nullptr,
               std::vector<const char*>* techniques = nullptr);
// Comprehensive solver using all available solving techniques
bool SolveComprehensive(Puzzle& puzzle);
// Solves puzzle and returns total difficulty score
// Score is sum of technique points across all solving steps
int SolveAndScore(const Puzzle& puzzle);
// Solves puzzle and returns detailed scoring breakdown
PuzzleScore SolveAndScoreDetailed(const Puzzle& puzzle);
// Headless consistency checks for detectors and generated hints.
int RunHintSelfChecks();

}  // namespace sudoku
