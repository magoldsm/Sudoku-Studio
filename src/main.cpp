#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "solver_test.h"
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

void OpenHelpInBrowser() {
  // Try multiple possible locations for the help file
  std::vector<std::string> candidates = {
    "docs/help/index.html",
    "../docs/help/index.html",
    "../../docs/help/index.html",
  };

  std::string helpPath;
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) {
      helpPath = std::filesystem::absolute(candidate).string();
      std::cerr << "Found help file at: " << helpPath << std::endl;
      break;
    }
  }

  if (helpPath.empty()) {
    std::cerr << "Error: Could not find help file at docs/help/index.html" << std::endl;
    return;
  }

  #ifdef _WIN32
    // Use file:// URL for Windows
    std::string fileUrl = "file:///" + helpPath;
    // Replace backslashes with forward slashes
    std::replace(fileUrl.begin(), fileUrl.end(), '\\', '/');
    std::string command = "start \"\" \"" + fileUrl + "\"";
    std::cerr << "Opening help with Windows command: " << command << std::endl;
    int result = system(command.c_str());
    if (result != 0) {
      std::cerr << "Warning: Failed to open help browser" << std::endl;
    }
  #else
    // Check if running in WSL
    bool isWSL = std::filesystem::exists("/proc/version") &&
                 std::system("grep -qi microsoft /proc/version 2>/dev/null") == 0;

    if (isWSL) {
      // In WSL, convert Unix path to Windows UNC path using wslpath
      // Run in background thread to avoid blocking the UI
      std::thread([helpPath]() {
        std::string wslpath_cmd = "wslpath -w \"" + helpPath + "\" 2>/dev/null";
        FILE* wslpath_pipe = popen(wslpath_cmd.c_str(), "r");
        std::string winPath;
        if (wslpath_pipe) {
          char buffer[512];
          if (fgets(buffer, sizeof(buffer), wslpath_pipe) != nullptr) {
            winPath = buffer;
            // Remove trailing newline
            if (!winPath.empty() && winPath.back() == '\n') {
              winPath.pop_back();
            }
          }
          pclose(wslpath_pipe);
        }

        // Convert Windows UNC path to file:// URL format
        if (!winPath.empty()) {
          std::replace(winPath.begin(), winPath.end(), '\\', '/');
          std::string fileUrl = "file://" + winPath;

          // Use cmd.exe to open the file:// URL in default browser
          // Redirect output and background the process so it doesn't block
          std::string command = "cmd.exe /c start \"\" \"" + fileUrl + "\" >/dev/null 2>&1 &";
          std::cerr << "Opening help in Windows browser..." << std::endl;
          system(command.c_str());
        } else {
          std::cerr << "Could not convert path to Windows format." << std::endl;
        }
      }).detach();
      return;
    }

    // Use file:// URL for Linux/macOS
    std::string fileUrl = "file://" + helpPath;
    std::string command = "xdg-open \"" + fileUrl + "\" 2>/dev/null || open \"" + fileUrl + "\" 2>/dev/null";
    std::cerr << "Opening help with: " << command << std::endl;
    int result = system(command.c_str());
    if (result != 0) {
      std::cerr << "Warning: Failed to open help browser" << std::endl;
    }
  #endif
}

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

// Set window icon from Windows resources (.rc file)
void SetWindowIcon(GLFWwindow* window) {
  // On Windows: Icon is embedded in .rc resource for File Explorer display
  // On macOS/Linux: Icons are handled via app bundle / desktop files
  // No runtime code needed to avoid triggering Smart App Control
  (void)window;  // Suppress unused parameter warning
}

// Normalize line endings: convert CRLF to LF for consistent handling
void NormalizeLineEndings(char* text, size_t maxLen) {
  if (!text) return;

  size_t readPos = 0;
  size_t writePos = 0;

  while (readPos < maxLen && text[readPos] != '\0') {
    if (text[readPos] == '\r' && readPos + 1 < maxLen && text[readPos + 1] == '\n') {
      // Skip the \r, keep the \n
      text[writePos++] = '\n';
      readPos += 2;
    } else if (text[readPos] == '\r') {
      // Standalone \r, convert to \n
      text[writePos++] = '\n';
      readPos++;
    } else {
      text[writePos++] = text[readPos++];
    }
  }
  text[writePos] = '\0';
}

// Detect if running on WSL
bool IsWSL() {
  FILE* fp = fopen("/proc/version", "r");
  if (!fp) return false;

  char line[256];
  bool wsl = false;
  if (fgets(line, sizeof(line), fp)) {
    // Check for "microsoft" or "WSL" in /proc/version
    if (strstr(line, "microsoft") || strstr(line, "WSL")) {
      wsl = true;
    }
  }
  fclose(fp);
  return wsl;
}

// Platform-specific clipboard copy that works reliably on Linux/WSL
bool CopyToSystemClipboard(const std::string& text) {
  #ifdef _WIN32
    // On native Windows, ImGui's SetClipboardText should work via GLFW
    ImGui::SetClipboardText(text.c_str());
    return true;
  #else
    // On WSL, use clip.exe directly (avoid X11 connection issues)
    static bool isWSL = IsWSL();
    if (isWSL) {
      FILE* pipe = popen("clip.exe", "w");
      if (pipe) {
        fwrite(text.c_str(), 1, text.size(), pipe);
        int ret = pclose(pipe);
        if (ret == 0) {
          return true;
        }
      }
      return false;
    }

    // On native Linux, try xclip/xsel (avoid if X11 is broken)
    // Try xclip first (most common)
    FILE* pipe = popen("xclip -selection clipboard", "w");
    if (pipe) {
      fwrite(text.c_str(), 1, text.size(), pipe);
      int ret = pclose(pipe);
      if (ret == 0) {
        return true;
      }
    }

    // Try xsel as fallback
    pipe = popen("xsel --clipboard --input", "w");
    if (pipe) {
      fwrite(text.c_str(), 1, text.size(), pipe);
      int ret = pclose(pipe);
      if (ret == 0) {
        return true;
      }
    }

    // Fallback: try ImGui's method anyway
    ImGui::SetClipboardText(text.c_str());
    return false;
  #endif
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

// Helper function to completely clear the current hint state
inline void ClearHint(UIState& uiState) {
  uiState.currentHint.techniqueName.clear();
  uiState.currentHint.affectedCells.clear();
  uiState.currentHint.involvedDigits.clear();
  uiState.currentHint.chainEndpoints.clear();
  uiState.currentHint.chainCells.clear();
  uiState.currentHint.revealPhase = 0;
  uiState.hintMissingCandidates.clear();
  uiState.statusMessage.clear();
  uiState.statusFrames = 0;
}

// Helper to apply a solver action with undo/status handling
template<typename SolverFunc>
inline void ApplySolverAction(UIState& uiState,
                              PuzzleState& puzzleState,
                              SolverFunc solverFunc,
                              const std::string& successPrefix,
                              const std::string& successSuffix,
                              const std::string& failMessage,
                              int statusFrames) {
  uiState.mode = InputMode::kDigit;
  const Grid previous = puzzleState.grid;
  const int result = solverFunc(puzzleState.grid);
  ClearHint(uiState);
  if (result > 0) {
    PushUndoState(uiState.undoHistory, previous);
    uiState.statusMessage = successPrefix + std::to_string(result) + successSuffix;
  } else {
    uiState.statusMessage = failMessage;
  }
  uiState.statusFrames = statusFrames;
}

void DrawBoard(UIState& uiState,
               PuzzleState& puzzleState,
               bool solved,
               int highlightDigit,
               ImFont* digitFont,
               ImFont* noteFont) {
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
    const bool isDoubleClick = (uiState.mode == InputMode::kColor &&
                                lastClickedRow == clampedRow &&
                                lastClickedCol == clampedCol &&
                                (currentTime - lastClickTime) < kDoubleClickThreshold);

    lastClickedRow = clampedRow;
    lastClickedCol = clampedCol;
    lastClickTime = currentTime;

    // In color mode: single-click to apply color, double-click to remove
    if (uiState.mode == InputMode::kColor) {
      Cell& clickedCell = puzzleState.grid[clampedRow][clampedCol];
      if (isDoubleClick) {
        // Double-click: remove color
        if (clickedCell.colorTag != 0) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          clickedCell.colorTag = 0;
          ClearHint(uiState);
        }
      } else {
        // Single-click: apply color (don't select the cell)
        if (clickedCell.colorTag != uiState.activeColor) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          clickedCell.colorTag = uiState.activeColor;
          ClearHint(uiState);
        }
      }
      // Don't select the cell in color mode
    } else {
      // Non-color modes: check if cell was already selected before updating selection
      const bool cellAlreadySelected =
          (clampedRow == uiState.selectedRow && clampedCol == uiState.selectedCol);
      uiState.selectedCol = clampedCol;
      uiState.selectedRow = clampedRow;

      Cell& clickedCell = puzzleState.grid[uiState.selectedRow][uiState.selectedCol];
      // In pencil mode, only toggle pencil marks if the cell was already selected
      if (uiState.mode == InputMode::kPencil && cellAlreadySelected && !clickedCell.fixed && clickedCell.value == 0) {
        const float localX = mouse.x - (origin.x + static_cast<float>(clickedCol) * kCellSize);
        const float localY = mouse.y - (origin.y + static_cast<float>(clickedRow) * kCellSize);
        const int subCol = std::clamp(static_cast<int>((localX / kCellSize) * 3.0f), 0, 2);
        const int subRow = std::clamp(static_cast<int>((localY / kCellSize) * 3.0f), 0, 2);
        const int markDigit = subRow * 3 + subCol + 1;
        if (clickedCell.pencil.test(markDigit - 1)) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          clickedCell.pencil.reset(markDigit - 1);
          ClearHint(uiState);
        }
      }
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();

  for (int r = 0; r < kGridSize; ++r) {
    for (int c = 0; c < kGridSize; ++c) {
      const ImVec2 cellMin(origin.x + c * kCellSize, origin.y + r * kCellSize);
      const ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);
      const bool hintCellActive = uiState.currentHint.revealPhase >= 2 && ContainsCell(uiState.currentHint.affectedCells, r, c);

      ImU32 fill = kCellBackgroundColor;
      if (SharesUnit(r, c, uiState.selectedRow, uiState.selectedCol)) {
        fill = kCellHoveredColor;
      }
      if (puzzleState.grid[r][c].colorTag > 0) {
        fill = kTagColors[puzzleState.grid[r][c].colorTag];
      }

      // Check if this cell has erased-but-legal candidates (validation warning)
      bool isValidationCell = false;
      for (const MissingCandidate& mc : uiState.currentHint.validation.erasedButLegal) {
        if (mc.row == r && mc.col == c) {
          isValidationCell = true;
          break;
        }
      }
      if (isValidationCell) {
        fill = kValidationWarningCellColor;
      }

      const int chainIndex = (uiState.currentHint.revealPhase >= 2) ? uiState.currentHint.ChainIndex(r, c) : -1;
      if (chainIndex >= 0) {
        // Chain cells get alternating colors (priority over general hint color)
        fill = (chainIndex % 2 == 0) ? kChainAlternateColorA : kChainAlternateColorB;
      } else if (hintCellActive) {
        // Elimination-target cells: use a warm amber when chain cells are also
        // present so targets stand out clearly from the blue/green chain cells.
        fill = uiState.currentHint.chainCells.empty() ? kCellHintColor : kCellHintEliminationColor;
      }
      if (r == uiState.selectedRow && c == uiState.selectedCol) {
        fill = kCellSelectedColor;
      }

      draw->AddRectFilled(cellMin, cellMax, fill);

      const int value = puzzleState.grid[r][c].value;
      if (uiState.currentHint.revealPhase >= 3 && hintCellActive && value != 0 && uiState.currentHint.IncludesDigit(value)) {
        draw->AddRect(ImVec2(cellMin.x + kCellEdgeInset, cellMin.y + kCellEdgeInset),
                      ImVec2(cellMax.x - kCellEdgeInset, cellMax.y - kCellEdgeInset),
                      kHintDigitHighlightColor, kCellBorderRadius, 0, kCellBorderThickness);
      }
      if (uiState.currentHint.revealPhase >= 2 && uiState.currentHint.IsChainEnd(r, c)) {
        draw->AddRect(ImVec2(cellMin.x + 1.0f, cellMin.y + 1.0f),
                      ImVec2(cellMax.x - 1.0f, cellMax.y - 1.0f),
                      kChainEndpointBorderColor, kChainEndpointBorderRadius, 0,
                      kChainEndpointBorderThickness);
      }
      if (highlightDigit != 0 && value == highlightDigit && !uiState.highlightPairs) {
        const ImVec2 center(cellMin.x + kCellSize * 0.5f, cellMin.y + kCellSize * 0.5f);
        draw->AddCircleFilled(center, kCellSize * kCircleRadiusFactor, kPencilHighlightBackgroundColor);
      }
      if (value != 0) {
        ImU32 textColor = puzzleState.grid[r][c].fixed ? kFixedDigitColor : kUserDigitColor;
        const bool hasConflict = IsConflict(puzzleState.grid, r, c, value);
        const bool wrongBySolution =
            puzzleState.hasSolution && !puzzleState.grid[r][c].fixed && value != puzzleState.solution[r][c];
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

        if (uiState.showWrongEntrySlash && !puzzleState.grid[r][c].fixed && (hasConflict || wrongBySolution)) {
          draw->AddLine(ImVec2(cellMin.x + 8.0f, cellMax.y - 8.0f),
                        ImVec2(cellMax.x - 8.0f, cellMin.y + 8.0f),
                        IM_COL32(220, 40, 40, 255), 4.0f);
        }
        continue;
      }

      for (int d = 1; d <= 9; ++d) {
        if (!puzzleState.grid[r][c].pencil.test(d - 1)) {
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
        if (highlightDigit == d || (uiState.highlightPairs && puzzleState.grid[r][c].pencil.count() == 2)) {
          draw->AddRectFilled(ImVec2(notePos.x - 2.0f, notePos.y - 1.0f),
                              ImVec2(notePos.x + noteSize.x + 2.0f, notePos.y + noteSize.y + 1.0f),
                              kPencilHighlightBackgroundColor, kPencilMarkBorderRadius);
        }
        if (uiState.currentHint.revealPhase >= 3 && hintCellActive && uiState.currentHint.IncludesDigit(d)) {
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

      // --- RENDER MISSING CANDIDATES (orange, for hint-affected cells) ---
      for (const MissingCandidate& mc : uiState.hintMissingCandidates) {
        if (mc.row == r && mc.col == c) {
          // This cell has missing candidates for the current hint
          for (int d : mc.missingDigits) {
            const int slotR = (d - 1) / 3;
            const int slotC = (d - 1) % 3;
            const std::string text = std::to_string(d);
            const ImVec2 notePos(cellMin.x + kPencilMarkStartX + slotC * kPencilMarkSpacingX,
                                 cellMin.y + kPencilMarkStartY + slotR * kPencilMarkSpacingY);
            const ImVec2 noteSize =
                noteFont->CalcTextSizeA(noteFont->FontSize, FLT_MAX, 0.0f, text.c_str());

            // Draw bright orange rectangle background
            draw->AddRectFilled(ImVec2(notePos.x - 2.0f, notePos.y - 1.0f),
                                ImVec2(notePos.x + noteSize.x + 2.0f, notePos.y + noteSize.y + 1.0f),
                                kMissingCandidateColor, kPencilMarkBorderRadius);

            // Draw orange text
            draw->AddText(noteFont,
                          noteFont->FontSize,
                          notePos,
                          IM_COL32(0, 0, 0, 255),  // black text on orange background
                          text.c_str());
          }
          break;
        }
      }
    }
  }

  // --- SHOW EFFECT SLASHES (for impossible cells) ---
  if (uiState.showEffectDigit != 0) {
    constexpr ImU32 slashColor = IM_COL32(30, 100, 210, 255);  // Blue
    constexpr float slashThickness = 3.0f;

    for (int r = 0; r < kGridSize; ++r) {
      for (int c = 0; c < kGridSize; ++c) {
        // Only show slashes in empty cells (value == 0) with pencil marks
        if (puzzleState.grid[r][c].value != 0) {
          continue;  // Skip cells with values
        }

        // Check if the digit is impossible (appears in same row/column/box)
        const int digit = uiState.showEffectDigit;
        bool isImpossible = false;

        // Check row
        for (int cc = 0; cc < kGridSize; ++cc) {
          if (puzzleState.grid[r][cc].value == digit) {
            isImpossible = true;
            break;
          }
        }

        // Check column
        if (!isImpossible) {
          for (int rr = 0; rr < kGridSize; ++rr) {
            if (puzzleState.grid[rr][c].value == digit) {
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
              if (puzzleState.grid[rr][cc].value == digit) {
                isImpossible = true;
                break;
              }
            }
            if (isImpossible) break;
          }
        }

        // Draw slash if digit is impossible and cell has pencil marks or is empty
        if (isImpossible && (puzzleState.grid[r][c].pencil.count() > 0 || puzzleState.grid[r][c].value == 0)) {
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
  DrawColorTagPanel(puzzleState.grid, uiState.activeColor, uiState.mode, kTagColors, uiState.showPositionDigit, uiState.showEffectDigit, uiState.highlightPairs);
}

// Create an OpenGL texture from RGBA pixel data
static GLuint CreateGLTexture(const unsigned char* data, int pixelSize) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixelSize, pixelSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  return tex;
}

// Returns the directory containing the running executable, with trailing slash.
// Uses argv[0] which is available from main() parameters.
static std::string g_exeDir = "./";

static void InitExeDir(const char* argv0) {
  try {
    std::filesystem::path exePath = std::filesystem::absolute(argv0);
    g_exeDir = exePath.parent_path().string() + "/";
  } catch (...) {
    g_exeDir = "./";
  }
}

static std::string GetExeDir() {
  return g_exeDir;
}

}  // namespace sudoku

using namespace sudoku;

// Forward declaration
int main(int argc, char** argv);

// WinMain stub for /SUBSYSTEM:WINDOWS (hides console on Windows)
#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  return main(__argc, __argv);
}
#endif

// Load window icon from PNG file
int main(int argc, char** argv) {
  InitExeDir(argv[0]);  // Initialize exe directory for asset loading

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
  if (argc > 1 && std::string(argv[1]) == "--debug-xychain") {
    std::string snapshotPath = argc > 2 ? argv[2] : "test_snapshots/xychain_false_positive.txt";
    DebugXYChain(snapshotPath);
    return 0;
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

  SetWindowIcon(window);
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

  // Create cursors and button icon textures from PNG images (lazy initialization)
  GLFWcursor* cursorPen = nullptr;
  GLFWcursor* cursorPencil = nullptr;
  GLFWcursor* cursorRainbow = nullptr;
  GLuint texPen = 0, texPencil = 0, texRainbow = 0;
  bool cursorsInitialized = false;

  while (!glfwWindowShouldClose(window)) {
    // Load cursor images from .rgba files on first loop iteration
    if (!cursorsInitialized) {
      const std::string assetDir = GetExeDir() + "assets/cursors/";
      constexpr int kIconSize = 64;
      constexpr int kIconBytes = kIconSize * kIconSize * 4;

      // Static buffers to hold image data (persist for lifetime of program)
      static unsigned char penData[kIconSize * kIconSize * 4] = {};
      static unsigned char pencilData[kIconSize * kIconSize * 4] = {};
      static unsigned char rainbowData[kIconSize * kIconSize * 4] = {};

      // Load Pen.rgba
      if (FILE* f = fopen((assetDir + "Pen.rgba").c_str(), "rb")) {
        fread(penData, 1, kIconBytes, f);
        fclose(f);
      }

      // Load Pencil.rgba
      if (FILE* f = fopen((assetDir + "Pencil.rgba").c_str(), "rb")) {
        fread(pencilData, 1, kIconBytes, f);
        fclose(f);
      }

      // Load Rainbow.rgba
      if (FILE* f = fopen((assetDir + "Rainbow.rgba").c_str(), "rb")) {
        fread(rainbowData, 1, kIconBytes, f);
        fclose(f);
      }

      GLFWimage penImg    = { kIconSize, kIconSize, penData };
      GLFWimage pencilImg = { kIconSize, kIconSize, pencilData };
      GLFWimage rainbowImg= { kIconSize, kIconSize, rainbowData };
      cursorPen     = glfwCreateCursor(&penImg,     11, 57);  // hotspot: nib tip (scaled 32→64)
      cursorPencil  = glfwCreateCursor(&pencilImg,  32, 62);  // hotspot: pencil tip (scaled)
      cursorRainbow = glfwCreateCursor(&rainbowImg, 32, 32);  // hotspot: center

      texPen     = CreateGLTexture(penData,     kIconSize);
      texPencil  = CreateGLTexture(pencilData,  kIconSize);
      texRainbow = CreateGLTexture(rainbowData, kIconSize);

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
          glfwSetCursor(window, cursorPen);
          break;
        case InputMode::kPencil:
          glfwSetCursor(window, cursorPencil);
          break;
        case InputMode::kColor:
          glfwSetCursor(window, cursorRainbow);
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
    bool requestTogglePairs = false;
    bool requestToggleSlash = false;

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
          requestTogglePairs = true;
        }
        if (ImGui::MenuItem("Wrong Entry Slash", nullptr, uiState.showWrongEntrySlash)) {
          requestToggleSlash = true;
        }
        if (ImGui::MenuItem(uiState.currentHint.revealPhase == 3 ? "Apply Hint" : "Hint", "?")) {
          if (uiState.currentHint.revealPhase == 3) requestApplyHint = true;
          else requestHint = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Help", "F1")) {
          OpenHelpInBrowser();
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
    // Mode selector radio buttons using cursor icon images
    const ImVec2 iconSz(34, 34);
    const auto modeBtn = [&](const char* id, GLuint tex, InputMode m) {
      if (uiState.mode == m) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
      }
      if (ImGui::ImageButton(id, (ImTextureID)(intptr_t)tex, iconSz))
        uiState.mode = m;
      ImGui::PopStyleColor();
    };
    modeBtn("##mode_digit",  texPen,     InputMode::kDigit);
    ImGui::SameLine(0.0f, 4.0f);
    modeBtn("##mode_pencil", texPencil,  InputMode::kPencil);
    ImGui::SameLine(0.0f, 4.0f);
    modeBtn("##mode_color",  texRainbow, InputMode::kColor);

    if (ImGui::Button("Auto Pencil", ImVec2(140, 34))) {
      requestAutoPencil = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Naked", ImVec2(95, 34))) {
      requestSolveNakedSingles = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button(uiState.autoSolveNakedSingles ? "Auto:On" : "Auto:Off", ImVec2(85, 34))) {
      uiState.autoSolveNakedSingles = !uiState.autoSolveNakedSingles;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hidden", ImVec2(95, 34))) {
      requestSolveHiddenSingles = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(uiState.showWrongEntrySlash ? "Slash On" : "Slash Off", ImVec2(105, 34))) {
      requestToggleSlash = true;
    }
    ImGui::SameLine();
    const bool hintAtApplyPhase = uiState.currentHint.revealPhase == 3;
    if (ImGui::Button(hintAtApplyPhase ? "Apply" : "Hint (?)", ImVec2(95, 34))) {
      if (hintAtApplyPhase) requestApplyHint = true;
      else requestHint = true;
    }
    if (uiState.currentHint.revealPhase > 0) {
      ImGui::SameLine(0.0f, 4.0f);
      if (ImGui::Button("X##cancel_hint", ImVec2(28, 34))) {
        ClearHint(uiState);
      }
    }

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
      requestTogglePairs = true;
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
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
      OpenHelpInBrowser();
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

      uiState.undoHistory.clear();
      ClearHint(uiState);
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
      ClearHint(uiState);
      if (UndoLastChange(puzzleState.grid, uiState.undoHistory)) {
        uiState.statusMessage = "Undid last change";
      } else {
        uiState.statusMessage = "Nothing to undo";
      }
      uiState.statusFrames = 200;
    }
    if (requestAutoPencil) {
      ApplySolverAction(uiState, puzzleState, [](Grid& g) { return ApplyAutoPencil(g); },
                        "Auto-pencil updated ", " cells", "No pencil changes", 240);
    }
    if (requestSolveNakedSingles) {
      ApplySolverAction(uiState, puzzleState, [](Grid& g) { return AutoSolveNakedSingles(g); },
                        "Solved ", " naked singles", "No naked singles found", 300);
    }
    if (requestSolveHiddenSingles) {
      ApplySolverAction(uiState, puzzleState, [](Grid& g) { return AutoSolveHiddenSingles(g); },
                        "Solved ", " hidden singles", "No hidden singles found", 300);
    }
    if (requestHint) {
      uiState.mode = InputMode::kDigit;  // Exit color mode
      if (uiState.currentHint.revealPhase == 0 || uiState.currentHint.revealPhase >= 3) {
        uiState.currentHint = GenerateHint(puzzleState.grid);
        uiState.currentHint.revealPhase = 1;

        // Compute missing candidates for hint-affected cells (only those relevant to the hint)
        uiState.hintMissingCandidates.clear();
        for (const HintCell& cell : uiState.currentHint.affectedCells) {
          const std::vector<int> missing = GetMissingCandidates(puzzleState.grid, cell.row, cell.col);
          // Filter: only keep missing candidates that are involved in this hint
          std::vector<int> relevantMissing;
          for (int d : missing) {
            if (std::find(uiState.currentHint.involvedDigits.begin(),
                          uiState.currentHint.involvedDigits.end(), d) !=
                uiState.currentHint.involvedDigits.end()) {
              relevantMissing.push_back(d);
            }
          }
          if (!relevantMissing.empty()) {
            uiState.hintMissingCandidates.push_back({cell.row, cell.col, relevantMissing});
          }
        }

        // Check if this is a validation warning (no solving hint found)
        if (!uiState.currentHint.validation.erasedButLegal.empty()) {
          uiState.statusMessage = "No solving hint available. Review your pencil marks.";
        } else {
          uiState.statusMessage = "Hint: " + uiState.currentHint.techniqueName;
        }
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
      // Use ApplyHintWithCandidates to ensure we apply with the same candidates the hint was detected with
      ApplyHintWithCandidates(puzzleState.grid, uiState.currentHint, uiState.currentHint.usedCandidates);
      uiState.statusMessage = "Applied: " + name;
      uiState.statusFrames = 300;
      ClearHint(uiState);
    }
    if (requestTogglePairs) {
      uiState.highlightPairs = !uiState.highlightPairs;
      uiState.statusMessage = uiState.highlightPairs ? "Pair highlight enabled" : "Pair highlight disabled";
      uiState.statusFrames = 200;
    }
    if (requestToggleSlash) {
      uiState.showWrongEntrySlash = !uiState.showWrongEntrySlash;
      uiState.statusMessage = uiState.showWrongEntrySlash ? "Wrong-entry slash enabled" : "Wrong-entry slash disabled";
      uiState.statusFrames = 200;
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
          ClearHint(uiState);
          // Auto-solve naked singles if enabled
          if (uiState.autoSolveNakedSingles) {
            ApplySolverAction(uiState, puzzleState,
                [](Grid& g) { return AutoSolveNakedSingles(g); },
                "Auto-solved ", " naked singles", "", 240);
          }
        }
      }
      if (IsClearPressed()) {
        if (selected.value != 0) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.value = 0;
          ClearHint(uiState);
        }
      }
    }
    if (editable && uiState.mode == InputMode::kPencil) {
      if (digit != 0 && selected.value == 0) {
        PushUndoState(uiState.undoHistory, puzzleState.grid);
        selected.pencil.flip(digit - 1);
        ClearHint(uiState);
      }
      if (IsClearPressed()) {
        if (selected.pencil.any()) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.pencil.reset();
          ClearHint(uiState);
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
            ClearHint(uiState);
          }
        } else {
          // Apply the active color
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.colorTag = uiState.activeColor;
          ClearHint(uiState);
        }
      }
      if (IsClearPressed()) {
        // Clear removes color from selected cell
        if (selected.colorTag != 0) {
          PushUndoState(uiState.undoHistory, puzzleState.grid);
          selected.colorTag = 0;
          ClearHint(uiState);
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

    DrawBoard(uiState, puzzleState, solved, highlightDigit, digitFont, noteFont);

    // Position Technique Panel to the right of Board/Color Tags (on same row)
    ImGui::SameLine(0.0f, kBoardPanelMargin);
    DrawTechniquePanel(uiState.currentHint, kTechniquePanelWidth);

    ImGui::Separator();
    ImGui::Text(
        "Controls: arrows/WASD move | Q/E/R modes | 1-9 input | 0/Backspace/Delete clear | Ctrl+Z undo | B pairs | ? hint");
    ImGui::Text("Selected: row %d col %d", uiState.selectedRow + 1, uiState.selectedCol + 1);

    if (solved) {
      ImGui::TextColored(ImVec4(0.18f, 0.53f, 0.31f, 1.0f), "Solved: grid is complete and valid.");
    } else if (uiState.statusFrames > 0 && !uiState.statusMessage.empty()) {
      // Show status message with priority (e.g., hint phases, solver actions, toggles)
      ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "%s", uiState.statusMessage.c_str());
    } else if (uiState.currentHint.revealPhase > 0 && !uiState.currentHint.techniqueName.empty()) {
      // Fallback: re-show hint status if message timed out but hint is still active
      ImGui::TextColored(ImVec4(0.36f, 0.26f, 0.18f, 1.0f), "Hint: %s%s",
                         uiState.currentHint.techniqueName.c_str(),
                         uiState.currentHint.revealPhase == 2 ? " (cells highlighted)" :
                         uiState.currentHint.revealPhase >= 3 ? " (digits highlighted)" : "");
    } else {
      ImGui::TextUnformatted("Hints active: unit, same digit, and matching pencil-mark highlight");
    }

    // Validation warning: erased-but-legal candidates (Stage 1 + Stage 2)
    if (!uiState.currentHint.validation.erasedButLegal.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Stage 1: You appear to have erased valid candidate(s). Review your work.");
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                         "Stage 2: Erased candidates still legal:");
      for (const MissingCandidate& mc : uiState.currentHint.validation.erasedButLegal) {
        std::string digits_str;
        for (int d : mc.missingDigits) {
          if (!digits_str.empty()) digits_str += ", ";
          digits_str += std::to_string(d);
        }
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                           "  r%dc%d: %s",
                           mc.row + 1, mc.col + 1, digits_str.c_str());
      }
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
        if (CopyToSystemClipboard(uiState.snapshotText)) {
          uiState.snapshotCopiedFeedback = 120;  // Show feedback for 120 frames (~2 seconds)
        }
      }
      ImGui::SameLine();
      if (uiState.snapshotCopiedFeedback > 0) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "✓ Copied to clipboard!");
        uiState.snapshotCopiedFeedback--;
      }
      ImGui::SameLine();
      if (ImGui::Button("Close", ImVec2(90.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::Spacing();
      // Use InputTextMultiline for selectable, copyable text (read-only mode)
      // Copy snapshot text to display buffer for ImGui
      if (!uiState.snapshotText.empty()) {
        const size_t len = std::min(uiState.snapshotText.size(), uiState.kLoadInputBufSize - 1);
        memcpy(uiState.snapshotDisplayBuf, uiState.snapshotText.c_str(), len);
        uiState.snapshotDisplayBuf[len] = '\0';
      }
      ImGui::InputTextMultiline("##snapshottext", uiState.snapshotDisplayBuf,
                                uiState.kLoadInputBufSize,
                                ImVec2(720.0f, 420.0f),
                                ImGuiInputTextFlags_ReadOnly);
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
          // Normalize line endings (CRLF -> LF) for consistent display in InputTextMultiline
          NormalizeLineEndings(uiState.loadInputBuf, uiState.kLoadInputBufSize);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Load", ImVec2(80.0f, 0.0f))) {
        // Normalize line endings before loading (in case user pasted directly into field)
        NormalizeLineEndings(uiState.loadInputBuf, uiState.kLoadInputBufSize);
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
          uiState.highlightPairs = loaded.highlightPairs;
          uiState.showWrongEntrySlash = loaded.showWrongEntrySlash;
          ClearHint(uiState);  // Clear hint; it may be stale if pencil marks changed
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
      // Data-driven score breakdown table
      struct ScoreRow {
        const char* label;
        int value;
        int pointsEach;
      };
      const ScoreRow rows[] = {
        {"Naked Singles",      sc.nakedSingles,         1},
        {"Hidden Singles",     sc.hiddenSingles,        3},
        {"Pointing Pairs",     sc.pointingPairs,       10},
        {"Box/Line Reduction", sc.boxLineReductions,   10},
        {"Naked Pairs",        sc.nakedPairs,          15},
        {"Hidden Pairs",       sc.hiddenPairs,         20},
        {"Naked Triples",      sc.nakedTriples,        30},
        {"Hidden Triples",     sc.hiddenTriples,       40},
        {"Naked Quads",        sc.nakedQuads,          50},
        {"Hidden Quads",       sc.hiddenQuads,         60},
        {"Block/Block",        sc.blockBlockInteractions, 25},
        {"X-Wing",             sc.xWings,              80},
        {"Unique Rectangle",   sc.uniqueRectangles,    70},
        {"Y-Wing",             sc.yWings,             100},
        {"Simple Colouring",   sc.simpleColourings,   120},
        {"Swordfish",          sc.swordfishes,        140},
        {"XYZ-Wing",           sc.xyzWings,           150},
        {"XY-Chain",           sc.xyChains,           200},
        {"Jellyfish",          sc.jellyfishes,        200},
        {"Forcing Chains",     sc.forcingChains,      300},
      };

      for (const auto& row : rows) {
        if (row.value > 0) {
          ImGui::Text("%-22s %3d x %3d = %4d", row.label, row.value / row.pointsEach,
                      row.pointsEach, row.value);
        }
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

  // Clean up cursors and textures (if they were created)
  if (cursorPen) glfwDestroyCursor(cursorPen);
  if (cursorPencil) glfwDestroyCursor(cursorPencil);
  if (cursorRainbow) glfwDestroyCursor(cursorRainbow);
  if (texPen) glDeleteTextures(1, &texPen);
  if (texPencil) glDeleteTextures(1, &texPencil);
  if (texRainbow) glDeleteTextures(1, &texRainbow);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
