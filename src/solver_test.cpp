#include "solver_test.h"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "hints_solver.h"
#include "sudoku_core.h"

namespace sudoku {

int CountUnsolvedCells(const Grid& grid) {
  int count = 0;
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      if (grid[r][c].value == 0) count++;
    }
  }
  return count;
}

std::string SolveWithLogging(Puzzle puzzle) {
  Grid grid = BuildGrid(puzzle);
  ApplyAutoPencil(grid);

  std::vector<const char*> appliedTechniques;
  constexpr int kMaxIterations = 200;

  for (int i = 0; i < kMaxIterations; ++i) {
    bool changed = false;

    // Exhaust each technique completely before moving to the next
    while (ApplyNakedSingles(grid) > 0) {
      appliedTechniques.push_back("Naked Singles");
      changed = true;
    }

    while (ApplyHiddenSingles(grid) > 0) {
      appliedTechniques.push_back("Hidden Singles");
      changed = true;
    }

    while (ApplyPointingPairs(grid) > 0) {
      appliedTechniques.push_back("Pointing Pairs");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyBoxLineReduction(grid) > 0) {
      appliedTechniques.push_back("Box/Line Reduction");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyNakedPairs(grid) > 0) {
      appliedTechniques.push_back("Naked Pairs");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyHiddenPairs(grid) > 0) {
      appliedTechniques.push_back("Hidden Pairs");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyNakedTriples(grid) > 0) {
      appliedTechniques.push_back("Naked Triples");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyHiddenTriples(grid) > 0) {
      appliedTechniques.push_back("Hidden Triples");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyNakedQuads(grid) > 0) {
      appliedTechniques.push_back("Naked Quads");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyHiddenQuads(grid) > 0) {
      appliedTechniques.push_back("Hidden Quads");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyBlockBlockInteraction(grid) > 0) {
      appliedTechniques.push_back("Block/Block");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyXWing(grid) > 0) {
      appliedTechniques.push_back("X-Wing");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyUniqueRectangle(grid) > 0) {
      appliedTechniques.push_back("Unique Rectangle");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyYWing(grid) > 0) {
      appliedTechniques.push_back("Y-Wing");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplySimpleColoring(grid) > 0) {
      appliedTechniques.push_back("Simple Colouring");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplySwordfish(grid) > 0) {
      appliedTechniques.push_back("Swordfish");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyXYZWing(grid) > 0) {
      appliedTechniques.push_back("XYZ-Wing");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyXYChain(grid) > 0) {
      appliedTechniques.push_back("XY-Chain");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyJellyfish(grid) > 0) {
      appliedTechniques.push_back("Jellyfish");
      ApplyNakedSingles(grid);
      ApplyHiddenSingles(grid);
      changed = true;
    }

    while (ApplyForcingChains(grid) > 0) {
      appliedTechniques.push_back("Forcing Chains");
      while (ApplyNakedSingles(grid) > 0 || ApplyHiddenSingles(grid) > 0) {
        // Keep propagating
      }
      changed = true;
    }

    if (!changed) break;
  }

  int remaining = CountUnsolvedCells(grid);
  std::string result;

  if (remaining == 0) {
    result = "OK";
  } else {
    result = "STUCK at " + std::to_string(remaining) + " cells";
  }

  // Add technique log
  if (!appliedTechniques.empty()) {
    result += " | Techniques: ";
    for (size_t i = 0; i < appliedTechniques.size(); ++i) {
      if (i > 0) result += ", ";
      result += appliedTechniques[i];
    }
  } else {
    result += " | No techniques applied";
  }

  return result;
}

int TestGenerationAndSolving() {
  std::cerr << "\n=== Puzzle Generation and Solving Test ===\n" << std::endl;

  std::random_device rd;
  std::mt19937 rng(rd());

  struct DifficultyTest {
    Difficulty difficulty;
    const char* name;
    int targetScoreMin, targetScoreMax;
    int puzzlesToTest;
  };

  const DifficultyTest tests[] = {
      {Difficulty::kSimple, "Simple", 0, 12, 3},
      {Difficulty::kEasy, "Easy", 8, 30, 3},
      {Difficulty::kMild, "Mild", 22, 80, 3},
      {Difficulty::kModerate, "Moderate", 60, 200, 3},
      {Difficulty::kHard, "Hard", 150, 500, 3},
      {Difficulty::kVeryHard, "Very Hard", 400, 1200, 2},
      {Difficulty::kFiendish, "Fiendish", 900, 3000, 2},
      {Difficulty::kDiabolical, "Diabolical", 2000, 100000, 1},
  };

  int totalGenerated = 0, totalSolved = 0, totalScoreMatches = 0, totalFailures = 0;

  for (const auto& test : tests) {
    std::cerr << "\n[" << test.name << "] Band: " << test.targetScoreMin << "-" << test.targetScoreMax << std::endl;

    for (int i = 0; i < test.puzzlesToTest; ++i) {
      Puzzle puzzle = GeneratePuzzleWithDifficulty(rng, test.difficulty);
      bool isEmpty = true;
      for (int r = 0; r < kGridSize && isEmpty; ++r) {
        for (int c = 0; c < kGridSize && isEmpty; ++c) {
          if (puzzle[r][c] != 0) isEmpty = false;
        }
      }
      if (isEmpty) {
        std::cerr << "  FAIL: Generation returned empty puzzle" << std::endl;
        totalFailures++;
        continue;
      }
      totalGenerated++;

      // Check solver can complete it with detailed logging
      std::string solveResult = SolveWithLogging(puzzle);
      if (solveResult.find("OK") != 0) {
        std::cerr << "  FAIL: " << solveResult << std::endl;
        totalFailures++;
        continue;
      }
      std::cerr << "  PASS: " << solveResult << std::endl;
      totalSolved++;

      // Check score
      int score = SolveAndScore(puzzle);
      if (score < test.targetScoreMin || score > test.targetScoreMax) {
        std::cerr << "    Score " << score << " outside band [" << test.targetScoreMin << ","
                  << test.targetScoreMax << "]" << std::endl;
      } else {
        totalScoreMatches++;
      }
    }
  }

  std::cerr << "\n=== Test Summary ===" << std::endl;
  std::cerr << "Generated: " << totalGenerated << std::endl;
  std::cerr << "Solved: " << totalSolved << " (" << (100 * totalSolved / (totalGenerated + 1)) << "%)" << std::endl;
  std::cerr << "Scores in band: " << totalScoreMatches << " (" << (100 * totalScoreMatches / (totalGenerated + 1))
            << "%)" << std::endl;
  std::cerr << "Failures: " << totalFailures << std::endl;

  return totalFailures > 0 ? 1 : 0;
}

int RunHintStressTest(int durationSeconds) {
  std::cerr << "\n=== Hint System Stress Test ===" << std::endl;
  if (durationSeconds > 0) {
    std::cerr << "Running for " << durationSeconds << " seconds..." << std::endl;
  } else {
    std::cerr << "Running indefinitely (Ctrl+C to stop)..." << std::endl;
  }

  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> difficultyDist(0, 7);

  auto startTime = std::chrono::steady_clock::now();
  int puzzlesCompleted = 0;
  int totalHints = 0;
  int errors = 0;

  while (true) {
    // Check time limit
    if (durationSeconds > 0) {
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - startTime)
          .count();
      if (elapsed >= durationSeconds) break;
    }

    // Generate puzzle at random difficulty
    Difficulty diff = static_cast<Difficulty>(difficultyDist(rng));
    Puzzle puzzle = GeneratePuzzleWithDifficulty(rng, diff);

    // Check puzzle is solvable
    Puzzle testPuzzle = puzzle;
    if (!SolveComprehensive(testPuzzle)) {
      std::cerr << "ERROR: Generated puzzle is not solvable!" << std::endl;
      errors++;
      continue;
    }

    // Initialize grid from puzzle values
    Grid grid = BuildGrid(puzzle);

    // Use ApplyAutoPencil to properly initialize candidates (same as UI)
    ApplyAutoPencil(grid);

    // Now solve using hints
    int hintCount = 0;
    while (!IsSolved(grid)) {
      Hint hint = GenerateHint(grid);

      if (hint.techniqueName.empty() || hint.techniqueName == "No supported Sadman hint available") {
        // Check if puzzle is actually solved
        if (!IsSolved(grid)) {
          std::cerr << "STUCK: No hint available but puzzle not solved after " << hintCount << " hints" << std::endl;

          // Diagnostic: what techniques are available?
          Grid testGrid = grid;
          bool foundSomething = false;

          if (ApplyNakedSingles(testGrid) > 0) {
            std::cerr << "  → NakedSingles would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyHiddenSingles(testGrid) > 0) {
            std::cerr << "  → HiddenSingles would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyPointingPairs(testGrid) > 0) {
            std::cerr << "  → PointingPairs would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyBoxLineReduction(testGrid) > 0) {
            std::cerr << "  → BoxLineReduction would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyNakedPairs(testGrid) > 0) {
            std::cerr << "  → NakedPairs would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyHiddenPairs(testGrid) > 0) {
            std::cerr << "  → HiddenPairs would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyNakedTriples(testGrid) > 0) {
            std::cerr << "  → NakedTriples would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyHiddenTriples(testGrid) > 0) {
            std::cerr << "  → HiddenTriples would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyNakedQuads(testGrid) > 0) {
            std::cerr << "  → NakedQuads would work" << std::endl;
            foundSomething = true;
          }
          testGrid = grid;
          if (ApplyHiddenQuads(testGrid) > 0) {
            std::cerr << "  → HiddenQuads would work" << std::endl;
            foundSomething = true;
          }

          if (!foundSomething) {
            std::cerr << "  → No techniques found (true dead end)" << std::endl;
          }

          errors++;
        }
        break;
      }

      // Apply hint
      if (ApplyHint(grid, hint) == 0) {
        std::cerr << "ERROR: Failed to apply hint: " << hint.techniqueName << std::endl;
        errors++;
        break;
      }

      // Update pencil marks using the same function as UI
      ApplyAutoPencil(grid);

      hintCount++;
      totalHints++;

      // Safety check - if we've used way too many hints, something is wrong
      if (hintCount > 500) {
        std::cerr << "ERROR: Exceeded 500 hints for single puzzle" << std::endl;
        errors++;
        break;
      }
    }

    puzzlesCompleted++;
    if (puzzlesCompleted % 10 == 0) {
      std::cerr << "Completed " << puzzlesCompleted << " puzzles, " << totalHints
                << " total hints, " << errors << " errors" << std::endl;
    }
  }

  std::cerr << "\n=== Hint Stress Test Results ===" << std::endl;
  std::cerr << "Puzzles completed: " << puzzlesCompleted << std::endl;
  std::cerr << "Total hints used: " << totalHints << std::endl;
  std::cerr << "Average hints per puzzle: "
            << (puzzlesCompleted > 0 ? (double)totalHints / puzzlesCompleted : 0) << std::endl;
  std::cerr << "Errors: " << errors << std::endl;

  return errors > 0 ? 1 : 0;
}

}  // namespace sudoku
