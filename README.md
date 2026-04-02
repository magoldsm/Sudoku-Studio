# Sudoku Studio (C++ + Dear ImGui)

Sudoku Studio is a desktop Sudoku app built with C++17, Dear ImGui, GLFW, and OpenGL.
It combines a puzzle workspace, interactive solving tools, and an advanced hint pipeline.

## Current Status

- **Generator and Solver Synchronized**: All 20 solving techniques use identical algorithms across generation, scoring, and solving pipelines. Generator validates 100% of puzzles are completely solvable before acceptance.
- **Complete Technique Coverage**: 20 deterministic solving techniques implemented and tested across all difficulty bands (Simple → Diabolical).
- Multi-file refactor complete with explicit headers and source modules.
- Snapshot save/load format is versioned (`sudoku-studio-v1`).
- Two validation modes: `--self-check` (hint detector consistency) and `--test-generation` (generator/solver synchronization).

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/sudoku_ui
```

## Run Tests

**Detector Self-Check** — validates hint detection consistency:
```bash
./build/sudoku_ui --self-check
```
Reports detector coverage and failures across generated puzzle states.

**Generator/Solver Synchronization Test** — validates puzzle generation and solving:
```bash
./build/sudoku_ui --test-generation
```
Generates 20 puzzles across all difficulty bands and verifies 100% are solvable to completion using the same techniques.

## Core Features

- **Puzzle Generation**: 8 difficulty levels (Simple 0–12 → Diabolical 2000+) with automatic difficulty scoring.
  - Unique-solution enforcement during clue removal
  - Score-gated generation (rejects clues that exceed difficulty ceiling)
  - Complete solvability validation before acceptance
- **Input Modes**: Digit, pencil, and color-tag modes with mode switching.
- **Solving Tools**: 
  - Auto-pencil (rebuild legal candidates)
  - Single-technique auto-solvers (Naked Singles, Hidden Singles)
  - Interactive hint system with multi-phase reveal and Apply button
- **Hint System**:
  - Phase 1: Technique name
  - Phase 2: Affected cells highlighted
  - Phase 3: Involved digits highlighted → "Apply" button to execute technique
  - Score breakdown popup showing all 20 techniques with frequency (count × points = total)
- **UI Features**:
  - Undo history
  - Pencil-pair highlight toggle
  - Wrong-entry slash overlay
  - Conflict highlighting (duplicate digits in row/col/box)
  - Technique progression panel
- **Data Persistence**: Snapshot export/import with versioned format (`sudoku-studio-v1`).

## Implemented Solving Techniques (20 Total)

**Basic** (score 1–3):
- Naked Single (1 pt)
- Hidden Single (3 pts)

**Intermediate** (score 10–40):
- Pointing Pair/Triple (10 pts)
- Box/Line Reduction (10 pts)
- Naked Pair (15 pts) / Hidden Pair (20 pts)
- Naked Triple (30 pts) / Hidden Triple (40 pts)

**Advanced Subsets** (score 50–60):
- Naked Quad (50 pts)
- Hidden Quad (60 pts)
- Block/Block Interaction (25 pts)

**Fish Patterns** (score 80–200):
- X-Wing (80 pts)
- Swordfish (140 pts)
- Jellyfish (200 pts)

**Complex Patterns** (score 70–300):
- Unique Rectangle (70 pts)
- Y-Wing (100 pts)
- Simple Colouring (120 pts)
- XYZ-Wing (150 pts)
- XY-Chain (200 pts)
- Forcing Chains (300 pts)

**Difficulty Bands** (total score determines puzzle difficulty):
- Simple: 0–12
- Easy: 8–30
- Mild: 22–80
- Moderate: 60–200
- Hard: 150–500
- Very Hard: 400–1200
- Fiendish: 900–3000
- Diabolical: 2000+

If no applicable technique is found, the hint system reports no available technique.

## Controls

- Mouse:
	- Left click selects a cell.
	- In pencil mode, clicking a displayed mini-digit clears that pencil mark.
- Movement:
	- Arrow keys or `W/A/S/D`.
- Mode switching:
	- `Q` for digit mode
	- `E` for pencil mode
	- `R` for color mode
- Solving tools:
	- `?` request/reveal hint step (cycles through phases; at phase 3, button text changes to "Apply")
	- `P` auto pencil
	- `K` solve naked singles
	- `H` solve hidden singles
- Other actions:
	- `Ctrl+Z` undo
	- `B` toggle pencil-pair highlight
	- `N` new puzzle
	- `1..9` enter value, toggle pencil, or set color depending on mode
	- `0`, `Backspace`, or `Delete` clear active mode content for selected cell
	- `Space` applies the active color tag in color mode

Given clues are immutable.

## Project Layout

- `src/sudoku_core.h`: shared data types (Grid, Cell, Puzzle, Hint, Difficulty) and core grid/puzzle helpers.
- `src/hints_solver.h` / `src/hints_solver.cpp`: 20 solving technique detectors and appliers, puzzle scoring, comprehensive solver, and validation.
- `src/ui_helpers.h` / `src/ui_helpers.cpp`: input handling, mode management, and UI rendering (technique panel, score display).
- `src/ui_constants.h`: UI layout constants (cell size, padding, colors, fonts).
- `src/app_state.h`: UI state management (selected cell, active hint, undo stack, mode).
- `src/snapshot_io.h` / `src/snapshot_io.cpp`: snapshot serialization/deserialization (versioned format with givens, values, pencils, colors, hints).
- `src/main.cpp`: ImGui application entry point, event loop, puzzle generation, and main workflow.

## Dependencies

Dependencies are fetched by CMake:

- GLFW (`3.4`)
- Dear ImGui (`v1.91.9b`)
- OpenGL
