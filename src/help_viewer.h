#pragma once

#include <string>
#include <vector>

namespace sudoku {

class HelpViewer {
 public:
  HelpViewer();
  ~HelpViewer() = default;

  // Load help content from HELP.md file
  bool LoadHelpContent(const std::string& helpFilePath);

  // Render the help window in ImGui
  void Render(bool& isOpen);

  // Simple markdown to text converter (removes markdown syntax)
  static std::string MarkdownToText(const std::string& markdown);

 private:
  std::string helpContent;
  char searchQuery[256] = {0};
  std::vector<int> searchResults;
};

}  // namespace sudoku
