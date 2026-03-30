# Sudoku Studio (C++ + Dear ImGui)

Sudoku Studio is a desktop Sudoku app built with C++17, Dear ImGui, GLFW, and OpenGL.
It combines a puzzle workspace, interactive solving tools, and an advanced hint pipeline.

## Current Status

- Multi-file refactor complete with explicit headers and source modules.
- Snapshot save/load format is versioned (`sudoku-studio-v1`).
- Headless validation mode is available via `--self-check`.
- Advanced hint techniques include `Block/Block Interaction` and `Forcing Chains`.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/sudoku_ui
```

## Run Self-Check

```bash
./build/sudoku_ui --self-check
```

This runs internal consistency checks across generated states and reports detector coverage and failures.

## Core Features

- Puzzle generation by selected difficulty (`Simple` through `Diabolical`).
- Unique-solution enforcement during clue removal.
- Digit, pencil, and color-tag input modes.
- Undo history.
- Auto-pencil refresh.
- Single-technique auto-solvers for naked and hidden singles.
- Multi-phase hint reveal:
	- Phase 1: technique name
	- Phase 2: affected cells
	- Phase 3: involved digits
- Technique panel showing progression and active technique.
- Wrong-entry slash overlay and conflict highlighting.
- Snapshot export/import for bug reports and state restoration.

## Implemented Hint Techniques

- Naked Single
- Hidden Single
- Pointing Pair/Triple
- Box/Line Reduction
- Naked Pair, Hidden Pair
- Naked Triple, Hidden Triple
- Naked Quad, Hidden Quad
- Block/Block Interaction
- X-Wing, Swordfish, Jellyfish
- Unique Rectangle
- Y-Wing, XYZ-Wing
- Simple Colouring
- XY-Chain
- Forcing Chains

If no supported hint applies, the UI reports that no supported Sadman hint is available.

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
	- `P` auto pencil
	- `K` solve naked singles
	- `H` solve hidden singles
	- `?` request/reveal hint step
- Other actions:
	- `Ctrl+Z` undo
	- `B` toggle pencil-pair highlight
	- `N` new puzzle
	- `1..9` enter value, toggle pencil, or set color depending on mode
	- `0`, `Backspace`, or `Delete` clear active mode content for selected cell
	- `Space` applies the active color tag in color mode

Given clues are immutable.

## Project Layout

- `src/sudoku_core.h`: shared data types and core grid/puzzle helpers.
- `src/hints_solver.h` and `src/hints_solver.cpp`: hint detectors, auto-solvers, and self-check logic.
- `src/ui_helpers.h` and `src/ui_helpers.cpp`: key input utilities and technique panel rendering.
- `src/snapshot_io.h` and `src/snapshot_io.cpp`: snapshot serialization and parsing.
- `src/main.cpp`: app entry point and main ImGui workflow.

## Dependencies

Dependencies are fetched by CMake:

- GLFW (`3.4`)
- Dear ImGui (`v1.91.9b`)
- OpenGL
