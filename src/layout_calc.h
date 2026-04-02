#pragma once

#include "sudoku_core.h"

namespace sudoku {

// ============================================================================
// LAYOUT CALCULATION: All dimensions derived from grid size and cell size
// ============================================================================

// Ground truths: grid size and cell dimensions
// kGridSize = 9 (from sudoku_core.h)
// kCellSize = 72.0f (from sudoku_core.h)
// kBoardSize = kCellSize * kGridSize = 648.0f (from sudoku_core.h)

namespace Layout {

// --- BOARD AREA (the grid + padding) ---
constexpr float kBoardAreaPaddingX = 6.0f;
constexpr float kBoardAreaPaddingY = 9.0f;
constexpr float kBoardAreaWidth = kBoardSize + 2.0f * kBoardAreaPaddingX;   // 660
constexpr float kBoardAreaHeight = kBoardSize + 2.0f * kBoardAreaPaddingY;  // 666

// --- COLOR TAGS PANEL ---
// 9 color buttons (1-9), increased height to fit DPI-scaled digit fonts
// At 1.5x DPI: digits need ~40-45px height
constexpr float kTagButtonWidth = 108.0f;
constexpr float kTagButtonHeight = 48.0f;   // Increased from 30 to fit scaled fonts
constexpr float kTagButtonMargin = 8.0f;
constexpr int kTagCount = 9;
// Panel width: button width + padding on sides
constexpr float kTagPanelWidth = kTagButtonWidth + 2.0f * 28.0f;  // ~164
// Panel height: match board area (so panels align vertically)
constexpr float kTagPanelHeight = kBoardAreaHeight;

// --- TECHNIQUE PANEL ---
// Scrollable list of techniques, width depends on technique name length
// Height matches board area to align with color tags
constexpr float kTechniquePanelWidth = 280.0f;  // Content-determined: technique names
constexpr float kTechniquePanelHeight = kBoardAreaHeight;

// --- HORIZONTAL PANEL SPACING ---
constexpr float kPanelMargin = 12.0f;

// --- WORKSPACE WINDOW (contains board + panels + text controls) ---
// Horizontal layout: [BoardArea] [ColorTags] [Technique]
// 660 + 12 + 164 + 12 + 280 = 1128
constexpr float kWorkspaceContentWidth =
    kBoardAreaWidth +
    kPanelMargin +
    kTagPanelWidth +
    kPanelMargin +
    kTechniquePanelWidth;  // 1128

// Vertical layout: [Title section] [Board + Panels] [Control section]
// Text line height accounts for DPI scaling (base ~24px, scales to 30-36px at 1.5x-1.5x DPI)
constexpr float kTextLineHeight = 32.0f;   // Increased to accommodate scaled fonts
constexpr float kSeparatorHeight = 12.0f;  // Increased spacing
constexpr float kSectionSpacingV = 12.0f;

// Title section: "Sudoku Studio - [Difficulty] Score: [Score]" + "Sharp text..." + spacing
constexpr float kTitleSectionHeight = 3.0f * kTextLineHeight + kSectionSpacingV;  // 108

// Control section: "Controls:" line, "Selected:" line, Status line + spacing
constexpr float kControlSectionHeight = 3.0f * kTextLineHeight + 2.0f * kSectionSpacingV;  // 120

// Total workspace content height:
// Title (108) + Board (666) + Separator (12) + Controls (120) + Padding (24) = 930
constexpr float kWorkspacePaddingV = 24.0f;   // top/bottom padding
constexpr float kWorkspaceContentHeight =
    kTitleSectionHeight +
    kBoardAreaHeight +
    kSeparatorHeight +
    kControlSectionHeight +
    kWorkspacePaddingV;  // 930

// Workspace window is positioned below menu + toolbar
constexpr float kWorkspaceWindowWidth = kWorkspaceContentWidth;
constexpr float kWorkspaceWindowHeight = kWorkspaceContentHeight;

// --- MENU BAR & TOOLBAR ---
constexpr float kMainMenuBarHeight = 24.0f;   // ImGui default menu bar
constexpr float kToolbarHeight = 96.0f;       // Custom toolbar buttons and controls
constexpr float kTopUIHeight = kMainMenuBarHeight + kToolbarHeight;  // 120

// --- GLFW WINDOW TOTAL ---
// Width: 1128 (workspace width)
// Height: 120 (menu 24 + toolbar 96) + 826 (workspace) = 946
constexpr float kWindowWidth = kWorkspaceWindowWidth;
constexpr float kWindowHeight = kTopUIHeight + kWorkspaceWindowHeight;

// --- WORKSPACE WINDOW POSITIONING ---
// Position below menu + toolbar
constexpr float kWorkspaceWindowX = 0.0f;
constexpr float kWorkspaceWindowY = kTopUIHeight;  // 120

}  // namespace Layout

}  // namespace sudoku
