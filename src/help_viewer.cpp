#include "help_viewer.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>
#include <imgui.h>

namespace sudoku {

HelpViewer::HelpViewer() = default;

bool HelpViewer::LoadHelpContent(const std::string& helpFilePath) {
  std::ifstream file(helpFilePath);
  if (!file.is_open()) {
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  helpContent = MarkdownToText(content);
  return true;
}

std::string HelpViewer::MarkdownToText(const std::string& markdown) {
  std::string text = markdown;

  // Static regex patterns compiled once
  static const std::regex headerPattern(R"(^#+\s+)");
  static const std::regex boldPattern(R"(\*\*([^*]+)\*\*)");
  static const std::regex italicPattern(R"(\*([^*]+)\*)");
  static const std::regex linkPattern(R"(\[([^\]]+)\]\([^\)]+\))");
  static const std::regex hrPattern(R"(^-{3,}$)");

  // Remove markdown headers (##, ###, etc.)
  text = std::regex_replace(text, headerPattern, "");

  // Remove markdown bold (**text**)
  text = std::regex_replace(text, boldPattern, "$1");

  // Remove markdown italic (*text*)
  text = std::regex_replace(text, italicPattern, "$1");

  // Remove markdown links [text](url)
  text = std::regex_replace(text, linkPattern, "$1");

  // Remove horizontal rules (---)
  text = std::regex_replace(text, hrPattern, "");

  return text;
}

void HelpViewer::Render(bool& isOpen) {
  if (!isOpen) {
    return;
  }

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 100, viewport->WorkPos.y + 100),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Help", &isOpen, ImGuiWindowFlags_NoMove)) {
    ImGui::End();
    return;
  }

  // Search box
  ImGui::InputTextWithHint("##helpSearch", "Search help...", searchQuery, sizeof(searchQuery));
  ImGui::Separator();

  // Help content
  if (ImGui::BeginChild("HelpContent", ImVec2(0, -40), true)) {
    if (helpContent.empty()) {
      ImGui::TextWrapped("Help content not loaded.");
    } else if (strlen(searchQuery) == 0) {
      ImGui::TextWrapped("%s", helpContent.c_str());
    } else {
      // Display search results
      std::string query = searchQuery;
      std::transform(query.begin(), query.end(), query.begin(), ::tolower);

      std::string lowerContent = helpContent;
      std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);

      if (lowerContent.find(query) != std::string::npos) {
        ImGui::TextWrapped("%s", helpContent.c_str());
      } else {
        ImGui::TextWrapped("No results found for '%s'", searchQuery);
      }
    }
    ImGui::EndChild();
  }

  // Close button
  if (ImGui::Button("Close", ImVec2(100, 0))) {
    isOpen = false;
  }

  ImGui::End();
}

}  // namespace sudoku
