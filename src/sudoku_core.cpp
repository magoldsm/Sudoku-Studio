#include "sudoku_core.h"

namespace sudoku {

const char* ModeName(InputMode mode) {
  switch (mode) {
    case InputMode::kDigit:
      return "Digit";
    case InputMode::kPencil:
      return "Pencil";
    case InputMode::kColor:
      return "Color";
  }
  return "Unknown";
}

const char* DifficultyName(Difficulty difficulty) {
  switch (difficulty) {
    case Difficulty::kSimple:
      return "Simple";
    case Difficulty::kEasy:
      return "Easy";
    case Difficulty::kMild:
      return "Mild";
    case Difficulty::kModerate:
      return "Moderate";
    case Difficulty::kHard:
      return "Hard";
    case Difficulty::kVeryHard:
      return "Very Hard";
    case Difficulty::kFiendish:
      return "Fiendish";
    case Difficulty::kDiabolical:
      return "Diabolical";
  }
  return "Unknown";
}

InputMode ParseMode(const char* modeStr) {
  if (!modeStr) return InputMode::kDigit;
  if (std::string(modeStr) == "Pencil") return InputMode::kPencil;
  if (std::string(modeStr) == "Color") return InputMode::kColor;
  return InputMode::kDigit;
}

Difficulty ParseDifficulty(const char* diffStr) {
  if (!diffStr) return Difficulty::kHard;
  std::string s(diffStr);
  if (s == "Simple") return Difficulty::kSimple;
  if (s == "Easy") return Difficulty::kEasy;
  if (s == "Mild") return Difficulty::kMild;
  if (s == "Moderate") return Difficulty::kModerate;
  if (s == "Hard") return Difficulty::kHard;
  if (s == "Very Hard") return Difficulty::kVeryHard;
  if (s == "Fiendish") return Difficulty::kFiendish;
  if (s == "Diabolical") return Difficulty::kDiabolical;
  return Difficulty::kHard;
}

}  // namespace sudoku
