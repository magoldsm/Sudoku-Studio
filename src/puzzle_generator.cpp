#include "sudoku_core.h"
#include "hints_solver.h"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace sudoku {

namespace {

bool FillSolvedBoard(Puzzle& board, int index, std::mt19937& rng) {
  if (index >= 81) {
    return true;
  }

  const int row = index / 9;
  const int col = index % 9;
  if (board[row][col] != 0) {
    return FillSolvedBoard(board, index + 1, rng);
  }

  std::array<int, 9> digits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::shuffle(digits.begin(), digits.end(), rng);

  for (int digit : digits) {
    if (!IsSafeInPuzzle(board, row, col, digit)) {
      continue;
    }
    board[row][col] = digit;
    if (FillSolvedBoard(board, index + 1, rng)) {
      return true;
    }
    board[row][col] = 0;
  }

  return false;
}

int CountSolutions(Puzzle& board, int limit) {
  int bestRow = -1;
  int bestCol = -1;
  int bestCount = 10;
  std::array<int, 9> bestDigits{};

  for (int row = 0; row < 9; ++row) {
    for (int col = 0; col < 9; ++col) {
      if (board[row][col] != 0) {
        continue;
      }

      std::array<int, 9> digits{};
      int count = 0;
      for (int d = 1; d <= 9; ++d) {
        if (IsSafeInPuzzle(board, row, col, d)) {
          digits[count++] = d;
        }
      }

      if (count == 0) {
        return 0;
      }

      if (count < bestCount) {
        bestCount = count;
        bestRow = row;
        bestCol = col;
        bestDigits = digits;
      }
    }
  }

  if (bestRow == -1) {
    return 1;
  }

  int total = 0;
  for (int i = 0; i < bestCount; ++i) {
    board[bestRow][bestCol] = bestDigits[i];
    total += CountSolutions(board, limit - total);
    if (total >= limit) {
      board[bestRow][bestCol] = 0;
      return total;
    }
  }
  board[bestRow][bestCol] = 0;
  return total;
}

}  // anonymous namespace

Puzzle GeneratePuzzleWithDifficulty(std::mt19937& rng, Difficulty difficulty) {
  constexpr int kMaxAttemptsPerThread = 200;
  constexpr int kNumThreads = 6;  // Performance cores only

  int targetMinClues = 24;
  targetMinClues = 17;

  // Map difficulty to required score ranges using the centralized table
  int targetScoreMin = 0;
  int targetScoreMax = 1000;
  const DifficultyBand* bands = GetDifficultyBands();
  for (int i = 0; i < GetDifficultyBandCount(); ++i) {
    if (bands[i].difficulty == difficulty) {
      targetScoreMin = bands[i].scoreMin;
      targetScoreMax = bands[i].scoreMax;
      break;
    }
  }

  std::atomic<bool> puzzleFound{false};
  Puzzle bestPuzzle{};
  std::mutex resultLock;
  std::atomic<int> totalAttempts{0};

  std::cerr << "Generating " << DifficultyName(difficulty) << " puzzle with "
            << kNumThreads << " threads (" << (kMaxAttemptsPerThread * kNumThreads)
            << " total attempts)..." << std::endl;

  // Worker function for each thread
  auto workerFn = [&](int threadId) {
    std::random_device rd;
    std::mt19937 threadRng(rd() + threadId);

    Puzzle best{};
    int bestClues = 81;
    int bestScoreDistance = 10000;
    int totalThreadAttempts = 0;

    for (int attempt = 0; attempt < kMaxAttemptsPerThread && !puzzleFound; ++attempt) {
      // Generate a random complete solved puzzle
      Puzzle full{};
      FillSolvedBoard(full, 0, threadRng);

      // Try removing clues
      Puzzle current = full;
      std::vector<std::pair<int, int>> positions;
      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          positions.push_back({r, c});
        }
      }
      std::shuffle(positions.begin(), positions.end(), threadRng);

      // Remove clues — score-gated so we never overshoot the target difficulty band.
      // After each successful removal we check the score; if it exceeds targetScoreMax
      // we put the clue back, preventing minimal-clue puzzles that are too hard.
      for (const auto& [r, c] : positions) {
        if (puzzleFound) return;

        int value = current[r][c];
        current[r][c] = 0;

        Puzzle check = current;
        if (CountSolutions(check, 2) != 1) {
          current[r][c] = value;
          continue;
        }

        // Removal keeps uniqueness — reject if it pushes score over the ceiling.
        if (SolveAndScore(current) > targetScoreMax) {
          current[r][c] = value;
        }
      }

      int clueCount = 0;
      for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
          if (current[r][c] != 0) clueCount++;
        }
      }

      totalThreadAttempts++;
      totalAttempts++;

      // Log progress every 50 total attempts per thread — no lock needed, cerr is thread-safe
      if (totalThreadAttempts % 50 == 0) {
        std::cerr << "[Thread " << threadId << "] Processed " << totalThreadAttempts
                  << " puzzles, best distance: " << bestScoreDistance << std::endl;
      }

      if (clueCount < targetMinClues) {
        continue;
      }

      int score = SolveAndScore(current);
      int scoreDistance = std::abs(score - (targetScoreMin + targetScoreMax) / 2);

      // Verify the puzzle is completely solvable, not just scored
      {
        Grid testGrid = BuildGrid(current);
        ApplyAutoPencil(testGrid);
        Puzzle tempPuzzle = current;
        if (!SolveComprehensive(tempPuzzle)) {
          continue;  // Reject unsolvable puzzles
        }
      }

      // Check puzzleFound less frequently
      if ((attempt % 10 == 0) && puzzleFound) {
        return;
      }

      if (score >= targetScoreMin && score <= targetScoreMax) {
        // Perfect match found
        {
          std::lock_guard<std::mutex> lock(resultLock);
          if (!puzzleFound) {
            std::cerr << "[Thread " << threadId << "] Found perfect match! Score: " << score
                      << " (" << clueCount << " clues)" << std::endl;
            bestPuzzle = current;
            puzzleFound = true;
          }
        }
        return;
      }

      if (clueCount >= targetMinClues && scoreDistance < bestScoreDistance) {
        best = current;
        bestClues = clueCount;
        bestScoreDistance = scoreDistance;
        std::cerr << "[Thread " << threadId << "] New best - Score: " << score
                  << " (distance: " << scoreDistance << ", " << clueCount << " clues)"
                  << std::endl;
      }
    }

    // If this thread found any candidate, store it as fallback
    if (bestScoreDistance < 10000) {
      std::lock_guard<std::mutex> lock(resultLock);
      if (!puzzleFound) {
        std::cerr << "[Thread " << threadId << "] Completed. Best score distance: "
                  << bestScoreDistance << std::endl;
        bestPuzzle = best;
        puzzleFound = true;
      }
    }
  };

  // Launch worker threads
  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(workerFn, i);
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  std::cerr << "Generation complete. Total attempts: " << totalAttempts << std::endl;

  // If no puzzle was found, return a blank puzzle
  if (!puzzleFound) {
    std::cerr << "Warning: Could not generate puzzle of difficulty "
              << DifficultyName(difficulty) << " after "
              << totalAttempts << " attempts." << std::endl;
    return {};
  }

  // Count clues in final puzzle
  int finalClues = 0;
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      if (bestPuzzle[r][c] != 0) finalClues++;
    }
  }
  int finalScore = SolveAndScore(bestPuzzle);

  std::cerr << "Generated " << DifficultyName(difficulty) << " puzzle: "
            << finalClues << " clues, score " << finalScore << std::endl;

  return bestPuzzle;
}

}  // namespace sudoku
