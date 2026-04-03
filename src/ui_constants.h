#pragma once

#include <imgui.h>
#include "layout_calc.h"

namespace sudoku {

// Window dimensions (computed from layout_calc.h)
constexpr float kWindowWidth = Layout::kWindowWidth;
constexpr float kWindowHeight = Layout::kWindowHeight;
constexpr float kToolbarHeight = Layout::kToolbarHeight;
constexpr float kMainMenuHeight = Layout::kMainMenuBarHeight;

// Board styling
constexpr ImU32 kCellBackgroundColor = IM_COL32(253, 251, 246, 255);
constexpr ImU32 kCellHoveredColor = IM_COL32(247, 236, 209, 255);
constexpr ImU32 kCellSelectedColor = IM_COL32(255, 228, 173, 255);
constexpr ImU32 kCellHintColor = IM_COL32(135, 206, 250, 200);
constexpr ImU32 kCellHintEliminationColor = IM_COL32(255, 190, 100, 210);  // amber: elimination targets
constexpr ImU32 kChainAlternateColorA = IM_COL32(138, 210, 255, 220);
constexpr ImU32 kChainAlternateColorB = IM_COL32(166, 235, 196, 220);
constexpr ImU32 kHintDigitHighlightColor = IM_COL32(255, 140, 0, 255);
constexpr ImU32 kChainEndpointBorderColor = IM_COL32(30, 100, 210, 255);
constexpr ImU32 kMissingCandidateColor = IM_COL32(255, 165, 0, 255);  // bright orange for missing pencil marks
constexpr ImU32 kValidationWarningCellColor = IM_COL32(255, 150, 150, 180);  // light red for validation warnings

// Grid lines
constexpr ImU32 kGridThickLineColor = IM_COL32(52, 44, 33, 255);
constexpr ImU32 kGridThinLineColor = IM_COL32(139, 126, 104, 255);
constexpr float kGridThickLineWidth = 3.2f;
constexpr float kGridThinLineWidth = 1.0f;

// Text colors
constexpr ImU32 kFixedDigitColor = IM_COL32(42, 36, 29, 255);
constexpr ImU32 kUserDigitColor = IM_COL32(34, 92, 126, 255);
constexpr ImU32 kConflictDigitColor = IM_COL32(192, 62, 62, 255);
constexpr ImU32 kSolvedDigitColor = IM_COL32(44, 128, 80, 255);
constexpr ImU32 kPencilMarkColor = IM_COL32(104, 96, 84, 255);
constexpr ImU32 kPencilHighlightColor = IM_COL32(35, 95, 130, 255);
constexpr ImU32 kPencilHighlightBackgroundColor = IM_COL32(0, 255, 0, 255);

// Pencil mark positioning
constexpr float kPencilMarkStartX = 10.0f;
constexpr float kPencilMarkStartY = 7.0f;
constexpr float kPencilMarkSpacingX = 23.0f;
constexpr float kPencilMarkSpacingY = 20.0f;

// Font sizes (scaled by DPI)
// Windows (high DPI): smaller base sizes that scale up nicely
// Linux/WSL (typically 1x DPI): larger base sizes for readability
#ifdef _WIN32
  // Windows: scaled by DPI (1.5x = 24pt, 2.0x = 32pt)
  constexpr float kUIFontSize = 16.0f;
  constexpr float kDigitFontSize = 24.0f;
  constexpr float kPencilMarkFontSize = 16.0f;
#else
  // Linux/WSL/macOS: typically 1x DPI, use original larger sizes
  constexpr float kUIFontSize = 22.0f;
  constexpr float kDigitFontSize = 40.0f;
  constexpr float kPencilMarkFontSize = 16.0f;
#endif

// Tag and Technique panels (computed from layout_calc.h)
constexpr float kTagPanelWidth = Layout::kTagPanelWidth;
constexpr float kTechniquePanelWidth = Layout::kTechniquePanelWidth;
constexpr float kTagButtonWidth = Layout::kTagButtonWidth;
constexpr float kTagButtonHeight = Layout::kTagButtonHeight;
constexpr ImVec4 kColorActiveButtonOverride = ImVec4(0.24f, 0.52f, 0.72f, 1.0f);

// Board panel margin and padding (computed from layout_calc.h)
constexpr float kBoardPanelMargin = Layout::kPanelMargin;
constexpr float kBoardPanelPaddingX = Layout::kBoardAreaPaddingX;
constexpr float kBoardPanelPaddingY = Layout::kBoardAreaPaddingY;

// Cell highlight radius
constexpr float kCellBorderRadius = 6.0f;
constexpr float kCellBorderThickness = 3.0f;

// Chain endpoint border (more prominent)
constexpr float kChainEndpointBorderRadius = 8.0f;
constexpr float kChainEndpointBorderThickness = 5.0f;

// Pencil mark highlight
constexpr float kPencilMarkBorderRadius = 3.0f;
constexpr float kPencilMarkBorderThickness = 2.0f;

// Circle highlight radius factor
constexpr float kCircleRadiusFactor = 0.34f;

// Cell edge insets for borders
constexpr float kCellEdgeInset = 4.0f;
constexpr float kChainEdgeInset = 2.0f;

// Background color
constexpr ImU32 kBackgroundColor = IM_COL32(242, 238, 231, 255);

}  // namespace sudoku
