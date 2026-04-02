#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>

#include "app_state.h"
#include "hints_solver.h"
#include "imgui.h"
#include "snapshot_io.h"
#include "sudoku_core.h"
#include "ui_constants.h"
#include "ui_helpers.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace sudoku {

constexpr size_t kMaxUndoHistory = 256;

constexpr std::array<ImU32, 10> kTagColors = {
    0U,
    IM_COL32(254, 205, 211, 255),  // 1: Light Red
    IM_COL32(254, 240, 138, 255),  // 2: Light Yellow
    IM_COL32(220, 252, 231, 255),  // 3: Light Green
    IM_COL32(224, 242, 254, 255),  // 4: Light Blue
    IM_COL32(233, 213, 255, 255),  // 5: Light Purple
    IM_COL32(254, 215, 170, 255),  // 6: Light Orange
    IM_COL32(200, 245, 255, 255),  // 7: Light Cyan (was similar blue)
    IM_COL32(255, 245, 200, 255),  // 8: Light Tan/Beige (was similar blue)
    IM_COL32(217, 249, 157, 255),  // 9: Light Green-Yellow
};

  void PushUndoState(std::vector<Grid>& undoHistory, const Grid& grid) {
    if (undoHistory.size() >= kMaxUndoHistory) {
      undoHistory.erase(undoHistory.begin());
    }
    undoHistory.push_back(grid);
  }

  bool UndoLastChange(Grid& grid, std::vector<Grid>& undoHistory) {
    if (undoHistory.empty()) {
      return false;
    }
    grid = undoHistory.back();
    undoHistory.pop_back();
    return true;
  }

bool IsSafeInPuzzle(const Puzzle& board, int row, int col, int digit) {
  for (int i = 0; i < kGridSize; ++i) {
    if (board[row][i] == digit || board[i][col] == digit) {
      return false;
    }
  }

  const int boxRow = (row / 3) * 3;
  const int boxCol = (col / 3) * 3;
  for (int r = boxRow; r < boxRow + 3; ++r) {
    for (int c = boxCol; c < boxCol + 3; ++c) {
      if (board[r][c] == digit) {
        return false;
      }
    }
  }
  return true;
}

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

bool SolveFirstSolution(Puzzle& board) {
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
        return false;
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
    return true;
  }

  for (int i = 0; i < bestCount; ++i) {
    board[bestRow][bestCol] = bestDigits[i];
    if (SolveFirstSolution(board)) {
      return true;
    }
  }

  board[bestRow][bestCol] = 0;
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

bool PlaceSingleIfUnique(Puzzle& board,
                         const std::array<std::array<std::bitset<9>, 9>, 9>& candidates,
                         int row,
                         int col,
                         int digit) {
  if (board[row][col] != 0 || !candidates[row][col].test(digit - 1)) {
    return false;
  }
  board[row][col] = digit;
  return true;
}

bool SolveWithSinglesOnly(Puzzle puzzle) {
  while (true) {
    bool changed = false;
    std::array<std::array<std::bitset<9>, 9>, 9> candidates{};

    for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
        if (puzzle[row][col] != 0) {
          continue;
        }
        for (int d = 1; d <= 9; ++d) {
          if (IsSafeInPuzzle(puzzle, row, col, d)) {
            candidates[row][col].set(d - 1);
          }
        }
        if (candidates[row][col].count() == 1) {
          for (int d = 1; d <= 9; ++d) {
            if (candidates[row][col].test(d - 1)) {
              puzzle[row][col] = d;
              changed = true;
              break;
            }
          }
        }
      }
    }

    for (int row = 0; row < 9; ++row) {
      for (int digit = 1; digit <= 9; ++digit) {
        int count = 0;
        int foundCol = -1;
        for (int col = 0; col < 9; ++col) {
          if (candidates[row][col].test(digit - 1)) {
            ++count;
            foundCol = col;
          }
        }
        if (count == 1) {
          changed = PlaceSingleIfUnique(puzzle, candidates, row, foundCol, digit) || changed;
        }
      }
    }

    for (int col = 0; col < 9; ++col) {
      for (int digit = 1; digit <= 9; ++digit) {
        int count = 0;
        int foundRow = -1;
        for (int row = 0; row < 9; ++row) {
          if (candidates[row][col].test(digit - 1)) {
            ++count;
            foundRow = row;
          }
        }
        if (count == 1) {
          changed = PlaceSingleIfUnique(puzzle, candidates, foundRow, col, digit) || changed;
        }
      }
    }

    for (int boxRow = 0; boxRow < 3; ++boxRow) {
      for (int boxCol = 0; boxCol < 3; ++boxCol) {
        for (int digit = 1; digit <= 9; ++digit) {
          int count = 0;
          int foundRow = -1;
          int foundCol = -1;
          for (int row = boxRow * 3; row < boxRow * 3 + 3; ++row) {
            for (int col = boxCol * 3; col < boxCol * 3 + 3; ++col) {
              if (candidates[row][col].test(digit - 1)) {
                ++count;
                foundRow = row;
                foundCol = col;
              }
            }
          }
          if (count == 1) {
            changed = PlaceSingleIfUnique(puzzle, candidates, foundRow, foundCol, digit) || changed;
          }
        }
      }
    }

    if (!changed) {
      break;
    }
  }

  for (int row = 0; row < 9; ++row) {
    for (int col = 0; col < 9; ++col) {
      if (puzzle[row][col] == 0) {
        return false;
      }
    }
  }
  return true;
}

Puzzle GenerateHardPuzzle(std::mt19937& rng) {
  constexpr int kMinClues = 24;
  constexpr int kMaxAttempts = 60;

  Puzzle best = {};
  int bestClues = 81;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Puzzle solved = {};
    FillSolvedBoard(solved, 0, rng);

    Puzzle puzzle = solved;
    std::vector<int> positions(81);
    for (int i = 0; i < 81; ++i) {
      positions[i] = i;
    }
    std::shuffle(positions.begin(), positions.end(), rng);

    int clues = 81;
    for (int idx : positions) {
      if (clues <= kMinClues) {
        break;
      }

      const int row = idx / 9;
      const int col = idx % 9;
      const int backup = puzzle[row][col];
      puzzle[row][col] = 0;

      Puzzle check = puzzle;
      if (CountSolutions(check, 2) != 1) {
        puzzle[row][col] = backup;
        continue;
      }
      --clues;
    }

    if (!SolveWithSinglesOnly(puzzle)) {
      return puzzle;
    }

    if (clues < bestClues) {
      best = puzzle;
      bestClues = clues;
    }
  }

  return best;
}

Puzzle GeneratePuzzleWithDifficulty(std::mt19937& rng, Difficulty difficulty) {
  constexpr int kMaxAttemptsPerThread = 200;
  constexpr int kNumThreads = 6;  // Performance cores only

  int targetMinClues = 24;
  int targetScoreMin = 0;
  int targetScoreMax = 1000;

  // Map difficulty to required score ranges
  // New scoring: Naked Single=1, Hidden Single=3, Pointing Pair=10, Box/Line=10,
  // Naked Pair=15, Hidden Pair=20, Naked Triple=30, Hidden Triple=40
  switch (difficulty) {
    case Difficulty::kSimple:
      targetMinClues = 17;
      targetScoreMin = 0;
      targetScoreMax = 12;
      break;
    case Difficulty::kEasy:
      targetMinClues = 17;
      targetScoreMin = 8;
      targetScoreMax = 30;
      break;
    case Difficulty::kMild:
      targetMinClues = 17;
      targetScoreMin = 22;
      targetScoreMax = 80;
      break;
    case Difficulty::kModerate:
      targetMinClues = 17;
      targetScoreMin = 60;
      targetScoreMax = 200;
      break;
    case Difficulty::kHard:
      targetMinClues = 17;
      targetScoreMin = 150;
      targetScoreMax = 500;
      break;
    case Difficulty::kVeryHard:
      targetMinClues = 17;
      targetScoreMin = 400;
      targetScoreMax = 1200;
      break;
    case Difficulty::kFiendish:
      targetMinClues = 17;
      targetScoreMin = 900;
      targetScoreMax = 3000;
      break;
    case Difficulty::kDiabolical:
      targetMinClues = 17;
      targetScoreMin = 2000;
      targetScoreMax = 100000;
      break;
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

Grid BuildGrid(const Puzzle& puzzle) {
  Grid grid{};
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      grid[r][c].value = puzzle[r][c];
      grid[r][c].fixed = (puzzle[r][c] != 0);
    }
  }
  // Initialize pencil marks with legal candidates for all empty cells.
  // This ensures hint detectors have complete candidate information.
  ApplyAutoPencil(grid);
  return grid;
}

bool ComputeSolutionFromGivens(const Puzzle& givens, Puzzle& solution) {
  Puzzle puzzle = givens;
  if (!SolveFirstSolution(puzzle)) {
    return false;
  }
  solution = puzzle;
  return true;
}

bool IsConflict(const Grid& grid, int row, int col, int value) {
  if (value == 0) {
    return false;
  }

  for (int c = 0; c < kGridSize; ++c) {
    if (c != col && grid[row][c].value == value) {
      return true;
    }
  }
  for (int r = 0; r < kGridSize; ++r) {
    if (r != row && grid[r][col].value == value) {
      return true;
    }
  }

  const int boxR = (row / 3) * 3;
  const int boxC = (col / 3) * 3;
  for (int r = boxR; r < boxR + 3; ++r) {
    for (int c = boxC; c < boxC + 3; ++c) {
      if ((r != row || c != col) && grid[r][c].value == value) {
        return true;
      }
    }
  }
  return false;
}

void RemoveDigitFromPeerPencils(Grid& grid, int row, int col, int digit) {
  if (digit < 1 || digit > 9) {
    return;
  }

  const int bit = digit - 1;

  for (int c = 0; c < kGridSize; ++c) {
    if (c == col || grid[row][c].value != 0) {
      continue;
    }
    grid[row][c].pencil.reset(bit);
  }

  for (int r = 0; r < kGridSize; ++r) {
    if (r == row || grid[r][col].value != 0) {
      continue;
    }
    grid[r][col].pencil.reset(bit);
  }

  const int boxR = (row / 3) * 3;
  const int boxC = (col / 3) * 3;
  for (int r = boxR; r < boxR + 3; ++r) {
    for (int c = boxC; c < boxC + 3; ++c) {
      if ((r == row && c == col) || grid[r][c].value != 0) {
        continue;
      }
      grid[r][c].pencil.reset(bit);
    }
  }
}

bool IsSolved(const Grid& grid) {
  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      const int value = grid[r][c].value;
      if (value == 0 || IsConflict(grid, r, c, value)) {
        return false;
      }
    }
  }
  return true;
}

std::bitset<9> ComputeCandidates(const Grid& grid, int row, int col) {
  std::bitset<9> candidates;
  if (grid[row][col].value != 0) {
    return candidates;
  }

  for (int digit = 1; digit <= 9; ++digit) {
    if (!IsConflict(grid, row, col, digit)) {
      candidates.set(digit - 1);
    }
  }
  return candidates;
}


void ConfigureStyle() {
  ImGui::StyleColorsLight();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.CellPadding = ImVec2(8.0f, 8.0f);

  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.94f, 0.90f, 1.0f);
  style.Colors[ImGuiCol_TitleBg] = ImVec4(0.42f, 0.34f, 0.26f, 1.0f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.29f, 0.22f, 1.0f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.89f, 0.84f, 0.76f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.84f, 0.77f, 0.67f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.67f, 0.56f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.57f, 0.48f, 1.0f);
}

void DrawBoard(Grid& grid,
               int& selectedRow,
               int& selectedCol,
               int highlightDigit,
               InputMode& mode,
               int& activeColor,
               bool highlightPairs,
               bool showWrongEntrySlash,
               const Puzzle* solution,
               bool hasSolution,
               std::vector<Grid>& undoHistory,
               bool solved,
               const Hint& hint,
               ImFont* digitFont,
               ImFont* noteFont,
               int& showPositionDigit,
               int& showEffectDigit) {
  ImGui::BeginChild("BoardArea", ImVec2(kBoardSize + 12.0f, kBoardSize + 18.0f), true,
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 boardMax(origin.x + kBoardSize, origin.y + kBoardSize);
  ImGui::InvisibleButton("board_canvas", ImVec2(kBoardSize, kBoardSize));

  // Track double-click for color mode
  static int lastClickedRow = -1;
  static int lastClickedCol = -1;
  static float lastClickTime = 0.0f;
  constexpr float kDoubleClickThreshold = 0.3f;  // 300ms

  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const int clickedCol = static_cast<int>((mouse.x - origin.x) / kCellSize);
    const int clickedRow = static_cast<int>((mouse.y - origin.y) / kCellSize);
    int clampedCol = std::clamp(clickedCol, 0, 8);
    int clampedRow = std::clamp(clickedRow, 0, 8);

    // Detect double-click in color mode
    const float currentTime = ImGui::GetTime();
    const bool isDoubleClick = (mode == InputMode::kColor &&
                                lastClickedRow == clampedRow &&
                                lastClickedCol == clampedCol &&
                                (currentTime - lastClickTime) < kDoubleClickThreshold);

    lastClickedRow = clampedRow;
    lastClickedCol = clampedCol;
    lastClickTime = currentTime;

    // In color mode: single-click to apply color, double-click to remove
    if (mode == InputMode::kColor) {
      Cell& clickedCell = grid[clampedRow][clampedCol];
      if (isDoubleClick) {
        // Double-click: remove color
        if (clickedCell.colorTag != 0) {
          PushUndoState(undoHistory, grid);
          clickedCell.colorTag = 0;
        }
      } else {
        // Single-click: apply color (don't select the cell)
        if (clickedCell.colorTag != activeColor) {
          PushUndoState(undoHistory, grid);
          clickedCell.colorTag = activeColor;
        }
      }
      // Don't select the cell in color mode
    } else {
      // Non-color modes: always select the cell
      selectedCol = clampedCol;
      selectedRow = clampedRow;

      Cell& clickedCell = grid[selectedRow][selectedCol];
      if (mode == InputMode::kPencil && !clickedCell.fixed && clickedCell.value == 0) {
        const float localX = mouse.x - (origin.x + static_cast<float>(clickedCol) * kCellSize);
        const float localY = mouse.y - (origin.y + static_cast<float>(clickedRow) * kCellSize);
        const int subCol = std::clamp(static_cast<int>((localX / kCellSize) * 3.0f), 0, 2);
        const int subRow = std::clamp(static_cast<int>((localY / kCellSize) * 3.0f), 0, 2);
        const int markDigit = subRow * 3 + subCol + 1;
        if (clickedCell.pencil.test(markDigit - 1)) {
          PushUndoState(undoHistory, grid);
          clickedCell.pencil.reset(markDigit - 1);
        }
      }
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();

  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      const ImVec2 cellMin(origin.x + c * kCellSize, origin.y + r * kCellSize);
      const ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);
      const bool hintCellActive = hint.revealPhase >= 2 && ContainsCell(hint.affectedCells, r, c);

      ImU32 fill = kCellBackgroundColor;
      if (SharesUnit(r, c, selectedRow, selectedCol)) {
        fill = kCellHoveredColor;
      }
      if (grid[r][c].colorTag > 0) {
        fill = kTagColors[grid[r][c].colorTag];
      }
      const int chainIndex = (hint.revealPhase >= 2) ? hint.ChainIndex(r, c) : -1;
      if (chainIndex >= 0) {
        // Chain cells get alternating colors (priority over general hint color)
        fill = (chainIndex % 2 == 0) ? kChainAlternateColorA : kChainAlternateColorB;
      } else if (hintCellActive) {
        // Elimination-target cells: use a warm amber when chain cells are also
        // present so targets stand out clearly from the blue/green chain cells.
        fill = hint.chainCells.empty() ? kCellHintColor : kCellHintEliminationColor;
      }
      if (r == selectedRow && c == selectedCol) {
        fill = kCellSelectedColor;
      }

      draw->AddRectFilled(cellMin, cellMax, fill);

      const int value = grid[r][c].value;
      if (hint.revealPhase >= 3 && hintCellActive && value != 0 && hint.IncludesDigit(value)) {
        draw->AddRect(ImVec2(cellMin.x + kCellEdgeInset, cellMin.y + kCellEdgeInset),
                      ImVec2(cellMax.x - kCellEdgeInset, cellMax.y - kCellEdgeInset),
                      kHintDigitHighlightColor, kCellBorderRadius, 0, kCellBorderThickness);
      }
      if (hint.revealPhase >= 2 && hint.IsChainEnd(r, c)) {
        draw->AddRect(ImVec2(cellMin.x + 1.0f, cellMin.y + 1.0f),
                      ImVec2(cellMax.x - 1.0f, cellMax.y - 1.0f),
                      kChainEndpointBorderColor, kChainEndpointBorderRadius, 0,
                      kChainEndpointBorderThickness);
      }
      if (highlightDigit != 0 && value == highlightDigit && !highlightPairs) {
        const ImVec2 center(cellMin.x + kCellSize * 0.5f, cellMin.y + kCellSize * 0.5f);
        draw->AddCircleFilled(center, kCellSize * kCircleRadiusFactor, kPencilHighlightBackgroundColor);
      }
      if (value != 0) {
        ImU32 textColor = grid[r][c].fixed ? kFixedDigitColor : kUserDigitColor;
        const bool hasConflict = IsConflict(grid, r, c, value);
        const bool wrongBySolution =
            hasSolution && solution != nullptr && !grid[r][c].fixed && value != (*solution)[r][c];
        if (hasConflict || wrongBySolution) {
          textColor = kConflictDigitColor;
        }
        if (solved) {
          textColor = kSolvedDigitColor;
        }

        const std::string text = std::to_string(value);
        const ImVec2 sz = digitFont->CalcTextSizeA(digitFont->FontSize, FLT_MAX, 0.0f, text.c_str());
        draw->AddText(digitFont,
                      digitFont->FontSize,
                      ImVec2(cellMin.x + (kCellSize - sz.x) * 0.5f, cellMin.y + (kCellSize - sz.y) * 0.5f),
                      textColor,
                      text.c_str());

        if (showWrongEntrySlash && !grid[r][c].fixed && (hasConflict || wrongBySolution)) {
          draw->AddLine(ImVec2(cellMin.x + 8.0f, cellMax.y - 8.0f),
                        ImVec2(cellMax.x - 8.0f, cellMin.y + 8.0f),
                        IM_COL32(220, 40, 40, 255), 4.0f);
        }
        continue;
      }

      for (int d = 1; d <= 9; ++d) {
        if (!grid[r][c].pencil.test(d - 1)) {
          continue;
        }
        const int slotR = (d - 1) / 3;
        const int slotC = (d - 1) % 3;
        const std::string text = std::to_string(d);
        ImU32 noteColor = (highlightDigit == d) ? kPencilHighlightColor : kPencilMarkColor;
        const ImVec2 notePos(cellMin.x + kPencilMarkStartX + slotC * kPencilMarkSpacingX,
                             cellMin.y + kPencilMarkStartY + slotR *kPencilMarkSpacingY);

        const ImVec2 noteSize =
            noteFont->CalcTextSizeA(noteFont->FontSize, FLT_MAX, 0.0f, text.c_str());
        if (highlightDigit == d || (highlightPairs && grid[r][c].pencil.count() == 2)) {
          draw->AddRectFilled(ImVec2(notePos.x - 2.0f, notePos.y - 1.0f),
                              ImVec2(notePos.x + noteSize.x + 2.0f, notePos.y + noteSize.y + 1.0f),
                              kPencilHighlightBackgroundColor, kPencilMarkBorderRadius);
        }
        if (hint.revealPhase >= 3 && hintCellActive && hint.IncludesDigit(d)) {
          draw->AddRect(ImVec2(notePos.x - 3.0f, notePos.y - 2.0f),
                        ImVec2(notePos.x + noteSize.x + 3.0f, notePos.y + noteSize.y + 2.0f),
                        kHintDigitHighlightColor, kPencilMarkBorderRadius, 0, kPencilMarkBorderThickness);
        }

        draw->AddText(noteFont,
                      noteFont->FontSize,
                      notePos,
                      noteColor,
                      text.c_str());
      }
    }
  }

  // --- SHOW EFFECT SLASHES (for impossible cells) ---
  if (showEffectDigit != 0) {
    constexpr ImU32 slashColor = IM_COL32(30, 100, 210, 255);  // Blue
    constexpr float slashThickness = 3.0f;

    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        // Only show slashes in empty cells (value == 0) with pencil marks
        if (grid[r][c].value != 0) {
          continue;  // Skip cells with values
        }

        // Check if the digit is impossible (appears in same row/column/box)
        const int digit = showEffectDigit;
        bool isImpossible = false;

        // Check row
        for (int cc = 0; cc < kGridSize; ++cc) {
          if (grid[r][cc].value == digit) {
            isImpossible = true;
            break;
          }
        }

        // Check column
        if (!isImpossible) {
          for (int rr = 0; rr < kGridSize; ++rr) {
            if (grid[rr][c].value == digit) {
              isImpossible = true;
              break;
            }
          }
        }

        // Check box
        if (!isImpossible) {
          const int boxR = (r / 3) * 3;
          const int boxC = (c / 3) * 3;
          for (int rr = boxR; rr < boxR + 3; ++rr) {
            for (int cc = boxC; cc < boxC + 3; ++cc) {
              if (grid[rr][cc].value == digit) {
                isImpossible = true;
                break;
              }
            }
            if (isImpossible) break;
          }
        }

        // Draw slash if digit is impossible and cell has pencil marks or is empty
        if (isImpossible && (grid[r][c].pencil.count() > 0 || grid[r][c].value == 0)) {
          const ImVec2 cellMin(origin.x + c * kCellSize, origin.y + r * kCellSize);
          const ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);
          draw->AddLine(ImVec2(cellMin.x + 8.0f, cellMax.y - 8.0f),
                        ImVec2(cellMax.x - 8.0f, cellMin.y + 8.0f),
                        slashColor, slashThickness);
        }
      }
    }
  }

  DrawGridLines(draw, origin, boardMax);

  ImGui::EndChild();

  ImGui::SameLine(0.0f, kBoardPanelMargin);
  DrawColorTagPanel(grid, activeColor, mode, kTagColors, showPositionDigit, showEffectDigit, highlightPairs);
}

}  // namespace sudoku

using namespace sudoku;

// Solve a puzzle with detailed logging of techniques attempted
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
    Grid grid;
    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        grid[r][c].value = puzzle[r][c];
        grid[r][c].fixed = (puzzle[r][c] != 0);
      }
    }

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

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--self-check") {
    return RunHintSelfChecks();
  }
  if (argc > 1 && std::string(argv[1]) == "--test-generation") {
    return TestGenerationAndSolving();
  }
  if (argc > 1 && std::string(argv[1]) == "--hint-stress-test") {
    int durationSeconds = 0;  // 0 means indefinitely
    if (argc > 2) {
      durationSeconds = std::atoi(argv[2]);
    }
    return RunHintStressTest(durationSeconds);
  }

  if (!glfwInit()) {
    std::cerr << "Error: Failed to initialize GLFW" << std::endl;
    return 1;
  }

  const char* glslVersion = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  GLFWwindow* window = glfwCreateWindow(
      static_cast<int>(kWindowWidth), static_cast<int>(kWindowHeight),
      "Sudoku Studio", nullptr, nullptr);
  if (window == nullptr) {
    std::cerr << "Error: Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // Disable ImGui cursor management

  ConfigureStyle();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  // Font search paths — include Windows, macOS, and Linux
  std::vector<std::string> fontCandidates = {
      // Windows
      "C:\\Windows\\Fonts\\segoeui.ttf",
      "C:\\Windows\\Fonts\\arial.ttf",
      "C:\\Windows\\Fonts\\verdana.ttf",
      // macOS
      "/Library/Fonts/Arial.ttf",
      "/System/Library/Fonts/Helvetica.ttc",
      // Linux
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
  };

  // Detect DPI scaling from monitor
  float dpiScale = 1.0f;
  GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
  if (primaryMonitor != nullptr) {
    float xScale, yScale;
    glfwGetMonitorContentScale(primaryMonitor, &xScale, &yScale);
    dpiScale = (xScale + yScale) / 2.0f;  // Average of x and y scale
    std::cerr << "Detected DPI scale: " << dpiScale << " (xScale: " << xScale << ", yScale: " << yScale << ")" << std::endl;
  } else {
    std::cerr << "Warning: Could not detect primary monitor, using default DPI scale of 1.0" << std::endl;
  }

  // Calculate DPI-scaled font sizes
  const float scaledUIFontSize = kUIFontSize * dpiScale;
  const float scaledDigitFontSize = kDigitFontSize * dpiScale;
  const float scaledPencilMarkFontSize = kPencilMarkFontSize * dpiScale;

  std::cerr << "Using scaled font sizes - UI: " << scaledUIFontSize << "pt, Digit: " << scaledDigitFontSize
            << "pt, Pencil: " << scaledPencilMarkFontSize << "pt" << std::endl;

  ImFont* uiFont = nullptr;
  ImFont* digitFont = nullptr;
  ImFont* noteFont = nullptr;

  for (const std::string& fontPath : fontCandidates) {
    // Check if file exists before trying to load (avoid ImGui assertion)
    if (!std::filesystem::exists(fontPath)) {
      continue;
    }

    if (uiFont == nullptr) {
      uiFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), scaledUIFontSize);
      if (uiFont != nullptr) {
        std::cerr << "Loaded UI font from: " << fontPath << std::endl;
      }
    }
    if (digitFont == nullptr) {
      digitFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), scaledDigitFontSize);
      if (digitFont != nullptr) {
        std::cerr << "Loaded digit font from: " << fontPath << std::endl;
      }
    }
    if (noteFont == nullptr) {
      noteFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), scaledPencilMarkFontSize);
      if (noteFont != nullptr) {
        std::cerr << "Loaded note font from: " << fontPath << std::endl;
      }
    }

    // All fonts loaded successfully
    if (uiFont != nullptr && digitFont != nullptr && noteFont != nullptr) {
      break;
    }
  }

  if (uiFont == nullptr || digitFont == nullptr || noteFont == nullptr) {
    std::cerr << "Warning: Could not load custom fonts. Using ImGui default font (scaled by DPI: " << dpiScale << "x)." << std::endl;
    ImFontConfig config;
    config.SizePixels = scaledUIFontSize;
    ImFont* defaultFont = io.Fonts->AddFontDefault(&config);
    if (uiFont == nullptr) uiFont = defaultFont;
    if (digitFont == nullptr) digitFont = defaultFont;
    if (noteFont == nullptr) noteFont = defaultFont;
  }

  std::random_device rd;
  std::mt19937 rng(rd());

  PuzzleState puzzleState;
  puzzleState.puzzle = GeneratePuzzleWithDifficulty(rng, Difficulty::kHard);
  puzzleState.givens = puzzleState.puzzle;
  puzzleState.grid = BuildGrid(puzzleState.puzzle);
  puzzleState.hasSolution = ComputeSolutionFromGivens(puzzleState.givens, puzzleState.solution);
  puzzleState.score = SolveAndScoreDetailed(puzzleState.puzzle);

  std::cerr << "Initial puzzle score: " << puzzleState.score.totalScore << std::endl;

  UIState uiState;
  uiState.selectedDifficulty = Difficulty::kHard;
  uiState.statusMessage = "Menu/toolbar enabled. Ctrl+Z undo, P auto-pencil, K naked singles, H hidden singles";
  uiState.statusFrames = 480;

  // Create standard cursors for different modes (lazy initialization)
  GLFWcursor* cursorArrow = nullptr;
  GLFWcursor* cursorCrosshair = nullptr;
  GLFWcursor* cursorHand = nullptr;
  bool cursorsInitialized = false;

  while (!glfwWindowShouldClose(window)) {
    // Create cursors on first loop iteration
    if (!cursorsInitialized) {
      cursorArrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
      cursorCrosshair = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
      cursorHand = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
      cursorsInitialized = true;
    }
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // Set cursor based on mode (when mode changes)
    // Note: This may not work on all platforms (Linux/WSL2 cursor handling is inconsistent)
    static InputMode lastMode = static_cast<InputMode>(-1);
    if (uiState.mode != lastMode) {
      lastMode = uiState.mode;

      switch (uiState.mode) {
        case InputMode::kDigit:
          glfwSetCursor(window, cursorCrosshair);
          break;
        case InputMode::kPencil:
          glfwSetCursor(window, cursorHand);
          break;
        case InputMode::kColor:
          glfwSetCursor(window, cursorArrow);
          break;
      }
    }

    ImGui::NewFrame();

    ImGui::PushFont(uiFont);

    // Workspace sizing variables
    static ImVec2 measuredWorkspaceSize(0, 0);
    static bool workspaceSizeMeasured = false;

    bool requestNewPuzzle = false;
    bool requestAutoPencil = false;
    bool requestSolveNakedSingles = false;
    bool requestSolveHiddenSingles = false;
    bool requestHint = false;
    bool requestApplyHint = false;
    bool requestUndo = false;
    bool requestSnapshot = false;
    bool requestLoad = false;

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::BeginMenu("New Puzzle")) {
          if (ImGui::MenuItem("Simple      (0-12)", nullptr, uiState.selectedDifficulty == Difficulty::kSimple)) {
            uiState.selectedDifficulty = Difficulty::kSimple;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Easy        (8-30)", nullptr, uiState.selectedDifficulty == Difficulty::kEasy)) {
            uiState.selectedDifficulty = Difficulty::kEasy;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Mild       (22-80)", nullptr, uiState.selectedDifficulty == Difficulty::kMild)) {
            uiState.selectedDifficulty = Difficulty::kMild;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Moderate  (60-200)", nullptr, uiState.selectedDifficulty == Difficulty::kModerate)) {
            uiState.selectedDifficulty = Difficulty::kModerate;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Hard     (150-500)", nullptr, uiState.selectedDifficulty == Difficulty::kHard)) {
            uiState.selectedDifficulty = Difficulty::kHard;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Very Hard (400-1200)", nullptr, uiState.selectedDifficulty == Difficulty::kVeryHard)) {
            uiState.selectedDifficulty = Difficulty::kVeryHard;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Fiendish (900-3000)", nullptr, uiState.selectedDifficulty == Difficulty::kFiendish)) {
            uiState.selectedDifficulty = Difficulty::kFiendish;
            requestNewPuzzle = true;
          }
          if (ImGui::MenuItem("Diabolical  (2000+)", nullptr, uiState.selectedDifficulty == Difficulty::kDiabolical)) {
            uiState.selectedDifficulty = Difficulty::kDiabolical;
            requestNewPuzzle = true;
          }
          ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !uiState.undoHistory.empty())) {
          requestUndo = true;
        }
        if (ImGui::MenuItem("Snapshot", nullptr)) {
          requestSnapshot = true;
        }
        if (ImGui::MenuItem("Load Snapshot", nullptr)) {
          requestLoad = true;
        }
        if (ImGui::MenuItem("Auto Pencil", "P")) {
          requestAutoPencil = true;
        }
        if (ImGui::MenuItem("Solve Naked Singles", "K")) {
          requestSolveNakedSingles = true;
        }
        if (ImGui::MenuItem("Solve Hidden Singles", "H")) {
          requestSolveHiddenSingles = true;
        }
        if (ImGui::MenuItem("Highlight Pencil Pairs", "B", uiState.highlightPairs)) {
          uiState.highlightPairs = !uiState.highlightPairs;
          uiState.statusMessage = uiState.highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
          uiState.statusFrames = 200;
        }
        if (ImGui::MenuItem("Wrong Entry Slash", nullptr, uiState.showWrongEntrySlash)) {
          uiState.showWrongEntrySlash = !uiState.showWrongEntrySlash;
          uiState.statusMessage = uiState.showWrongEntrySlash ? "Wrong-entry slash enabled" : "Wrong-entry slash disabled";
          uiState.statusFrames = 200;
        }
        if (ImGui::MenuItem(uiState.currentHint.revealPhase == 3 ? "Apply Hint" : "Hint", "?")) {
          if (uiState.currentHint.revealPhase == 3) requestApplyHint = true;
          else requestHint = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Mode")) {
        if (ImGui::MenuItem("Digit", "Q", uiState.mode == InputMode::kDigit)) {
          uiState.mode = InputMode::kDigit;
        }
        if (ImGui::MenuItem("Pencil", "E", uiState.mode == InputMode::kPencil)) {
          uiState.mode = InputMode::kPencil;
        }
        if (ImGui::MenuItem("Color", "R", uiState.mode == InputMode::kColor)) {
          uiState.mode = InputMode::kColor;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImVec2(0, 24), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 96), ImGuiCond_Always);
    ImGui::Begin("Toolbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::Button("New", ImVec2(90, 34))) {
      requestNewPuzzle = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo", ImVec2(90, 34)) && !uiState.undoHistory.empty()) {
      requestUndo = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Snapshot", ImVec2(110, 34))) {
      requestSnapshot = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load", ImVec2(75, 34))) {
      requestLoad = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    int difficultyIdx = static_cast<int>(uiState.selectedDifficulty);
    const char* difficultyItems[] = {
      "Simple      (0-12)",
      "Easy        (8-30)",
      "Mild       (22-80)",
      "Moderate  (60-200)",
      "Hard     (150-500)",
      "Very Hard (400-1200)",
      "Fiendish (900-3000)",
      "Diabolical  (2000+)",
    };
    if (ImGui::Combo("##difficulty", &difficultyIdx, difficultyItems, 8)) {
      uiState.selectedDifficulty = static_cast<Difficulty>(difficultyIdx);
    }
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::Text("Mode: %s", ModeName(uiState.mode));

    if (ImGui::Button("Auto Pencil", ImVec2(140, 34))) {
      requestAutoPencil = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Naked", ImVec2(95, 34))) {
      requestSolveNakedSingles = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hidden", ImVec2(95, 34))) {
      requestSolveHiddenSingles = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(uiState.highlightPairs ? "Pairs On" : "Pairs Off", ImVec2(110, 34))) {
      uiState.highlightPairs = !uiState.highlightPairs;
      uiState.statusMessage = uiState.highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
      uiState.statusFrames = 200;
    }
    ImGui::SameLine();
    if (ImGui::Button(uiState.showWrongEntrySlash ? "Slash On" : "Slash Off", ImVec2(105, 34))) {
      uiState.showWrongEntrySlash = !uiState.showWrongEntrySlash;
      uiState.statusMessage = uiState.showWrongEntrySlash ? "Wrong-entry slash enabled" : "Wrong-entry slash disabled";
      uiState.statusFrames = 200;
    }
    ImGui::SameLine();
    const bool hintAtApplyPhase = uiState.currentHint.revealPhase == 3;
    if (ImGui::Button(hintAtApplyPhase ? "Apply" : "Hint (?)", ImVec2(95, 34))) {
      if (hintAtApplyPhase) requestApplyHint = true;
      else requestHint = true;
    }
    ImGui::SameLine(0.0f, 22.0f);
    ImGui::Text("Q/E/R switch mode");

    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
      uiState.mode = InputMode::kDigit;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      uiState.mode = InputMode::kPencil;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      uiState.mode = InputMode::kColor;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
      requestAutoPencil = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_K)) {
      requestSolveNakedSingles = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H)) {
      requestSolveHiddenSingles = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_B)) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      uiState.highlightPairs = !uiState.highlightPairs;
      uiState.statusMessage = uiState.highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
      uiState.statusFrames = 200;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_N)) {
      requestNewPuzzle = true;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      requestUndo = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Slash)) {
      if (uiState.currentHint.revealPhase == 3) requestApplyHint = true;
      else requestHint = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_D)) {
      uiState.selectedCol = (uiState.selectedCol + 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_A)) {
      uiState.selectedCol = (uiState.selectedCol + kGridSize - 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_S)) {
      uiState.selectedRow = (uiState.selectedRow + 1) % kGridSize;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_W)) {
      uiState.selectedRow = (uiState.selectedRow + kGridSize - 1) % kGridSize;
    }

    if (requestNewPuzzle) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      puzzleState.puzzle = GeneratePuzzleWithDifficulty(rng, uiState.selectedDifficulty);
      puzzleState.givens = puzzleState.puzzle;
      puzzleState.grid = BuildGrid(puzzleState.puzzle);
      puzzleState.hasSolution = ComputeSolutionFromGivens(puzzleState.givens, puzzleState.solution);
      puzzleState.score = SolveAndScoreDetailed(puzzleState.puzzle);

      // Debug output
      std::cerr << "Generated puzzle score: " << puzzleState.score.totalScore << std::endl;
      std::cerr << "  Naked Singles: " << puzzleState.score.nakedSingles << std::endl;
      std::cerr << "  Hidden Singles: " << puzzleState.score.hiddenSingles << std::endl;
      std::cerr << "  Pointing Pairs: " << puzzleState.score.pointingPairs << std::endl;
      std::cerr << "  Box/Line: " << puzzleState.score.boxLineReductions << std::endl;
      std::cerr << "  Naked Pairs: " << puzzleState.score.nakedPairs << std::endl;
      std::cerr << "  Hidden Pairs: " << puzzleState.score.hiddenPairs << std::endl;
      std::cerr << "  Naked Triples: " << puzzleState.score.nakedTriples << std::endl;
      std::cerr << "  Hidden Triples: " << puzzleState.score.hiddenTriples << std::endl;
      std::cerr << "  Naked Quads: " << puzzleState.score.nakedQuads << std::endl;
      std::cerr << "  Hidden Quads: " << puzzleState.score.hiddenQuads << std::endl;
      std::cerr << "  Block/Block: " << puzzleState.score.blockBlockInteractions << std::endl;
      std::cerr << "  X-Wing: " << puzzleState.score.xWings << std::endl;
      std::cerr << "  Unique Rectangle: " << puzzleState.score.uniqueRectangles << std::endl;
      std::cerr << "  Y-Wing: " << puzzleState.score.yWings << std::endl;
      std::cerr << "  Simple Colouring: " << puzzleState.score.simpleColourings << std::endl;
      std::cerr << "  Swordfish: " << puzzleState.score.swordfishes << std::endl;
      std::cerr << "  XYZ-Wing: " << puzzleState.score.xyzWings << std::endl;
      std::cerr << "  XY-Chain: " << puzzleState.score.xyChains << std::endl;
      std::cerr << "  Jellyfish: " << puzzleState.score.jellyfishes << std::endl;
      std::cerr << "  Forcing Chains: " << puzzleState.score.forcingChains << std::endl;

      uiState.undoHistory.clear();
      uiState.currentHint = {};
      uiState.selectedRow = 0;
      uiState.selectedCol = 0;
      uiState.statusMessage = std::string("Generated new ") + DifficultyName(uiState.selectedDifficulty) + " puzzle";
      uiState.statusFrames = 240;
    }
    if (requestSnapshot) {
      uiState.snapshotText = SerializeSnapshot(puzzleState.grid, uiState.selectedDifficulty, uiState.mode,
                                              uiState.selectedRow, uiState.selectedCol,
                                              uiState.highlightPairs, uiState.showWrongEntrySlash, uiState.currentHint);
      uiState.openSnapshotPopup = true;
      uiState.statusMessage = "Snapshot captured";
      uiState.statusFrames = 200;
    }
    if (requestUndo) {
      uiState.currentHint = {};
      if (UndoLastChange(puzzleState.grid, uiState.undoHistory)) {
        uiState.statusMessage = "Undid last change";
      } else {
        uiState.statusMessage = "Nothing to undo";
      }
      uiState.statusFrames = 200;
    }
    if (requestAutoPencil) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      const Grid previous = puzzleState.grid;
      const int changed = ApplyAutoPencil(puzzleState.grid);
      uiState.currentHint = {};
      if (changed > 0) {
        PushUndoState(uiState.undoHistory, previous);
      }
      uiState.statusMessage = "Auto-pencil updated " + std::to_string(changed) + " cells";
      uiState.statusFrames = 240;
    }
    if (requestSolveNakedSingles) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      const Grid previous = puzzleState.grid;
      const int placed = AutoSolveNakedSingles(puzzleState.grid);
      uiState.currentHint = {};
      if (placed > 0) {
        PushUndoState(uiState.undoHistory, previous);
      }
      if (placed > 0) {
        uiState.statusMessage = "Solved " + std::to_string(placed) + " naked singles";
      } else {
        uiState.statusMessage = "No naked singles found";
      }
      uiState.statusFrames = 300;
    }
    if (requestSolveHiddenSingles) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      const Grid previous = puzzleState.grid;
      const int placed = AutoSolveHiddenSingles(puzzleState.grid);
      uiState.currentHint = {};
      if (placed > 0) {
        PushUndoState(uiState.undoHistory, previous);
      }
      if (placed > 0) {
        uiState.statusMessage = "Solved " + std::to_string(placed) + " hidden singles";
      } else {
        uiState.statusMessage = "No hidden singles found";
      }
      uiState.statusFrames = 300;
    }
    if (requestHint) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      if (uiState.currentHint.revealPhase == 0 || uiState.currentHint.revealPhase >= 3) {
        uiState.currentHint = GenerateHint(puzzleState.grid);
        uiState.currentHint.revealPhase = 1;
        uiState.statusMessage = "Hint: " + uiState.currentHint.techniqueName;
        uiState.statusFrames = 300;
        uiState.hintPhaseCounter = 0;
      } else if (uiState.currentHint.revealPhase == 1) {
        uiState.currentHint.revealPhase = 2;
        uiState.statusMessage = "Hint: " + uiState.currentHint.techniqueName + " (cells highlighted)";
        uiState.statusFrames = 400;
      } else if (uiState.currentHint.revealPhase == 2) {
        uiState.currentHint.revealPhase = 3;
        uiState.statusMessage = "Hint: " + uiState.currentHint.techniqueName + " (digits highlighted)";
        uiState.statusFrames = 400;
      }
    }
    if (requestApplyHint && uiState.currentHint.revealPhase == 3) {
      const std::string name = uiState.currentHint.techniqueName;
      PushUndoState(uiState.undoHistory, puzzleState.grid);
      ApplyHint(puzzleState.grid, uiState.currentHint);
      uiState.statusMessage = "Applied: " + name;
      uiState.statusFrames = 300;
      uiState.currentHint = {};
    }

    Cell& selected = puzzleState.grid[uiState.selectedRow][uiState.selectedCol];
    const bool editable = !selected.fixed;
    const int digit = ReadDigitPress();

    if (editable && uiState.mode == InputMode::kDigit) {
      if (digit != 0) {
        if (selected.value != digit || selected.pencil.any()) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.value = digit;
          selected.pencil.reset();
          RemoveDigitFromPeerPencils(puzzleState.grid, uiState.selectedRow, uiState.selectedCol, digit);
          uiState.currentHint = {};
        }
      }
      if (IsClearPressed()) {
        if (selected.value != 0) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.value = 0;
          uiState.currentHint = {};
        }
      }
    }
    if (editable && uiState.mode == InputMode::kPencil) {
      if (digit != 0 && selected.value == 0) {
        PushUndoState(uiState.undoHistory, puzzleState.grid);
        selected.pencil.flip(digit - 1);
        uiState.currentHint = {};
      }
      if (IsClearPressed()) {
        if (selected.pencil.any()) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.pencil.reset();
          uiState.currentHint = {};
        }
      }
    }
    if (uiState.mode == InputMode::kColor) {
      if (digit != 0) {
        // In color mode, digits just change the active color
        uiState.activeColor = digit;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        // Space toggles the active color on/off on the selected cell
        if (selected.colorTag == uiState.activeColor) {
          // Already has this color, remove it
          if (selected.colorTag != 0) {
            PushUndoState(uiState.undoHistory, puzzleState.grid);
            selected.colorTag = 0;
            uiState.currentHint = {};
          }
        } else {
          // Apply the active color
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.colorTag = uiState.activeColor;
          uiState.currentHint = {};
        }
      }
      if (IsClearPressed()) {
        // Clear removes color from selected cell
        if (selected.colorTag != 0) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.colorTag = 0;
          uiState.currentHint = {};
        }
      }
    }

    const bool solved = IsSolved(puzzleState.grid);
    const int highlightDigit = uiState.showPositionDigit;  // Only highlight via the dot buttons

    ImGui::SetNextWindowPos(ImVec2(Layout::kWorkspaceWindowX, Layout::kWorkspaceWindowY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(Layout::kWorkspaceWindowWidth, Layout::kWorkspaceWindowHeight), ImGuiCond_Always);
    ImGui::Begin("Sudoku Workspace", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Sudoku Studio - %s", DifficultyName(uiState.selectedDifficulty));
    ImGui::SameLine();

    // Clickable score display
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    if (ImGui::Button(("Score: " + std::to_string(puzzleState.score.totalScore)).c_str())) {
      uiState.openScoreDetailsPopup = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::TextColored(ImVec4(0.33f, 0.27f, 0.20f, 1.0f),
                       "Sharp text rendering + menu/toolbar workflow");

    DrawBoard(puzzleState.grid, uiState.selectedRow, uiState.selectedCol, highlightDigit, uiState.mode,
              uiState.activeColor, uiState.highlightPairs, uiState.showWrongEntrySlash,
              &puzzleState.solution, puzzleState.hasSolution, uiState.undoHistory, solved,
              uiState.currentHint, digitFont, noteFont, uiState.showPositionDigit, uiState.showEffectDigit);

    // Position Technique Panel to the right of Board/Color Tags (on same row)
    ImGui::SameLine(0.0f, kBoardPanelMargin);
    DrawTechniquePanel(uiState.currentHint, kTechniquePanelWidth);

    ImGui::Separator();
    ImGui::Text(
        "Controls: arrows/WASD move | Q/E/R modes | 1-9 input | 0/Backspace/Delete clear | Ctrl+Z undo | B pairs | ? hint");
    ImGui::Text("Selected: row %d col %d", uiState.selectedRow + 1, uiState.selectedCol + 1);

    if (solved) {
      ImGui::TextColored(ImVec4(0.18f, 0.53f, 0.31f, 1.0f), "Solved: grid is complete and valid.");
    } else if (uiState.currentHint.revealPhase > 0 && !uiState.currentHint.techniqueName.empty()) {
      if (uiState.currentHint.revealPhase == 1) {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s",
                           uiState.currentHint.techniqueName.c_str());
      } else if (uiState.currentHint.revealPhase == 2) {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s (cells highlighted)",
                           uiState.currentHint.techniqueName.c_str());
      } else {
        ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s (digits highlighted)",
                           uiState.currentHint.techniqueName.c_str());
      }
    } else if (uiState.statusFrames > 0) {
      ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "%s", uiState.statusMessage.c_str());
    } else {
      ImGui::TextUnformatted("Hints active: unit, same digit, and matching pencil-mark highlight");
    }

    if (uiState.openSnapshotPopup) {
      ImGui::OpenPopup("Board Snapshot");
      uiState.openSnapshotPopup = false;
    }
    if (requestLoad) {
      uiState.loadErrorMessage.clear();
      ImGui::OpenPopup("Load Snapshot");
    }

    if (ImGui::BeginPopupModal("Board Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Copy this snapshot into chat with a description of the bug.");
      ImGui::Spacing();
      if (ImGui::Button("Copy to Clipboard", ImVec2(160.0f, 0.0f))) {
        ImGui::SetClipboardText(uiState.snapshotText.c_str());
      }
      ImGui::SameLine();
      if (ImGui::Button("Close", ImVec2(90.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::Spacing();
      ImGui::BeginChild("SnapshotText", ImVec2(720.0f, 420.0f), true,
                        ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::TextUnformatted(uiState.snapshotText.c_str());
      ImGui::EndChild();
      ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Load Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Paste a snapshot below, then click Load.");
      ImGui::Spacing();
      ImGui::InputTextMultiline("##loadbuf", uiState.loadInputBuf, uiState.kLoadInputBufSize,
                                ImVec2(720.0f, 380.0f));
      ImGui::Spacing();
      if (!uiState.loadErrorMessage.empty()) {
        ImGui::TextColored(ImVec4(0.8f, 0.15f, 0.15f, 1.0f), "%s", uiState.loadErrorMessage.c_str());
        ImGui::Spacing();
      }
      if (ImGui::Button("Paste from Clipboard", ImVec2(190.0f, 0.0f))) {
        const char* cb = ImGui::GetClipboardText();
        if (cb) {
          const size_t len = std::min(strlen(cb), uiState.kLoadInputBufSize - 1);
          memcpy(uiState.loadInputBuf, cb, len);
          uiState.loadInputBuf[len] = '\0';
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Load", ImVec2(80.0f, 0.0f))) {
        const SnapshotLoadResult loaded = DeserializeSnapshot(uiState.loadInputBuf);
        if (!loaded.ok) {
          uiState.loadErrorMessage = loaded.errorMessage;
        } else {
          uiState.undoHistory.clear();
          puzzleState.grid = loaded.grid;
          puzzleState.givens = loaded.givens;
          puzzleState.hasSolution = ComputeSolutionFromGivens(puzzleState.givens, puzzleState.solution);

          // Re-score the puzzle based on current state
          Puzzle currentPuzzle{};
          for (int r = 0; r < kGridSize; ++r) {
            for (int c = 0; c < kGridSize; ++c) {
              currentPuzzle[r][c] = puzzleState.givens[r][c];
            }
          }
          puzzleState.score = SolveAndScoreDetailed(currentPuzzle);

          uiState.selectedDifficulty = loaded.difficulty;
          uiState.mode = loaded.mode;
          uiState.selectedRow = std::clamp(loaded.selectedRow, 0, 8);
          uiState.selectedCol = std::clamp(loaded.selectedCol, 0, 8);
          uiState.currentHint = {};  // Clear hint; it may be stale if pencil marks changed
          uiState.loadErrorMessage.clear();
          memset(uiState.loadInputBuf, 0, uiState.kLoadInputBufSize);
          uiState.statusMessage = "Snapshot loaded";
          uiState.statusFrames = 240;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f))) {
        uiState.loadErrorMessage.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    if (uiState.openScoreDetailsPopup) {
      ImGui::OpenPopup("Puzzle Score Details");
      uiState.openScoreDetailsPopup = false;
    }

    if (ImGui::BeginPopupModal("Puzzle Score Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Puzzle Difficulty Breakdown");
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Total Score: %d", puzzleState.score.totalScore);
      ImGui::Spacing();

      const auto& sc = puzzleState.score;
      if (sc.nakedSingles > 0) {
        ImGui::Text("Naked Singles:      %3d x   1 = %4d", sc.nakedSingles / 1,   sc.nakedSingles);
      }
      if (sc.hiddenSingles > 0) {
        ImGui::Text("Hidden Singles:     %3d x   3 = %4d", sc.hiddenSingles / 3,  sc.hiddenSingles);
      }
      if (sc.pointingPairs > 0) {
        ImGui::Text("Pointing Pairs:     %3d x  10 = %4d", sc.pointingPairs / 10, sc.pointingPairs);
      }
      if (sc.boxLineReductions > 0) {
        ImGui::Text("Box/Line Reduction: %3d x  10 = %4d", sc.boxLineReductions / 10, sc.boxLineReductions);
      }
      if (sc.nakedPairs > 0) {
        ImGui::Text("Naked Pairs:        %3d x  15 = %4d", sc.nakedPairs / 15,    sc.nakedPairs);
      }
      if (sc.hiddenPairs > 0) {
        ImGui::Text("Hidden Pairs:       %3d x  20 = %4d", sc.hiddenPairs / 20,   sc.hiddenPairs);
      }
      if (sc.nakedTriples > 0) {
        ImGui::Text("Naked Triples:      %3d x  30 = %4d", sc.nakedTriples / 30,  sc.nakedTriples);
      }
      if (sc.hiddenTriples > 0) {
        ImGui::Text("Hidden Triples:     %3d x  40 = %4d", sc.hiddenTriples / 40, sc.hiddenTriples);
      }
      if (sc.nakedQuads > 0) {
        ImGui::Text("Naked Quads:        %3d x  50 = %4d", sc.nakedQuads / 50,    sc.nakedQuads);
      }
      if (sc.hiddenQuads > 0) {
        ImGui::Text("Hidden Quads:       %3d x  60 = %4d", sc.hiddenQuads / 60,   sc.hiddenQuads);
      }
      if (sc.blockBlockInteractions > 0) {
        ImGui::Text("Block/Block:        %3d x  25 = %4d", sc.blockBlockInteractions / 25, sc.blockBlockInteractions);
      }
      if (sc.xWings > 0) {
        ImGui::Text("X-Wing:             %3d x  80 = %4d", sc.xWings / 80,        sc.xWings);
      }
      if (sc.uniqueRectangles > 0) {
        ImGui::Text("Unique Rectangle:   %3d x  70 = %4d", sc.uniqueRectangles / 70, sc.uniqueRectangles);
      }
      if (sc.yWings > 0) {
        ImGui::Text("Y-Wing:             %3d x 100 = %4d", sc.yWings / 100,       sc.yWings);
      }
      if (sc.simpleColourings > 0) {
        ImGui::Text("Simple Colouring:   %3d x 120 = %4d", sc.simpleColourings / 120, sc.simpleColourings);
      }
      if (sc.swordfishes > 0) {
        ImGui::Text("Swordfish:          %3d x 140 = %4d", sc.swordfishes / 140,  sc.swordfishes);
      }
      if (sc.xyzWings > 0) {
        ImGui::Text("XYZ-Wing:           %3d x 150 = %4d", sc.xyzWings / 150,     sc.xyzWings);
      }
      if (sc.xyChains > 0) {
        ImGui::Text("XY-Chain:           %3d x 200 = %4d", sc.xyChains / 200,     sc.xyChains);
      }
      if (sc.jellyfishes > 0) {
        ImGui::Text("Jellyfish:          %3d x 200 = %4d", sc.jellyfishes / 200,  sc.jellyfishes);
      }
      if (sc.forcingChains > 0) {
        ImGui::Text("Forcing Chains:     %3d x 300 = %4d", sc.forcingChains / 300, sc.forcingChains);
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::Button("Close", ImVec2(90.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // Measure workspace window size before ending it
    ImVec2 windowSize = ImGui::GetWindowSize();

    ImGui::End();

    // Record the measured size (update each frame to account for content changes)
    if (windowSize.x > 0 && windowSize.y > 0) {
      measuredWorkspaceSize = windowSize;
      workspaceSizeMeasured = true;
    }

    if (uiState.statusFrames > 0) {
      --uiState.statusFrames;
    }

    ImGui::PopFont();

    ImGui::Render();
    int displayW = 0;
    int displayH = 0;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.95f, 0.93f, 0.89f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  // Clean up cursors (if they were created)
  if (cursorArrow) glfwDestroyCursor(cursorArrow);
  if (cursorCrosshair) glfwDestroyCursor(cursorCrosshair);
  if (cursorHand) glfwDestroyCursor(cursorHand);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
