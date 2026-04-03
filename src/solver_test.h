#pragma once

#include "sudoku_core.h"

namespace sudoku {

// Count the number of unsolved cells in a grid
int CountUnsolvedCells(const Grid& grid);

// Solve a puzzle with detailed logging of techniques attempted
std::string SolveWithLogging(Puzzle puzzle);

// Test puzzle generation and solving across all difficulty levels
int TestGenerationAndSolving();

// Stress test the hint system with generated puzzles
int RunHintStressTest(int durationSeconds = 0);

// Debug XY-Chain detection with detailed logging
void DebugXYChain(const std::string& snapshotPath);

}  // namespace sudoku
