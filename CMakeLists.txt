cmake_minimum_required(VERSION 3.16)
project(SudokuUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw imgui)

find_package(OpenGL REQUIRED)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

add_executable(sudoku_ui
  src/main.cpp
  src/hints_solver.cpp
  src/hints_candidates.cpp
  src/ui_helpers.cpp
  src/snapshot_io.cpp
  src/help_viewer.cpp
)
target_link_libraries(sudoku_ui PRIVATE imgui_lib)

if(UNIX AND NOT APPLE)
  target_link_libraries(sudoku_ui PRIVATE dl pthread)
endif()

# CPack configuration for NSIS installer
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "Sudoku Studio")
set(CPACK_PACKAGE_VENDOR "Sudoku Studio")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sudoku puzzle solver and hint system")
set(CPACK_PACKAGE_DESCRIPTION "Sudoku Studio is a comprehensive Sudoku puzzle solver with an intelligent hint system supporting 20+ solving techniques.")

# NSIS specific settings
set(CPACK_NSIS_PACKAGE_NAME "Sudoku Studio")
set(CPACK_NSIS_DISPLAY_NAME "Sudoku Studio v1.0.0")
set(CPACK_NSIS_INSTALLER_ICON "${CMAKE_SOURCE_DIR}/docs/icon.ico")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/docs/icon.ico")
set(CPACK_NSIS_INSTALLED_IMAGE_RESOURCE_NUM "106")
set(CPACK_NSIS_HELP_LINK "https://github.com/yourusername/sudoku-studio")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/yourusername/sudoku-studio")
set(CPACK_NSIS_CONTACT "support@sudoku-studio.local")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
  "CreateDirectory '$SMPROGRAMS\\\\Sudoku Studio'"
  "CreateShortCut '$SMPROGRAMS\\\\Sudoku Studio\\\\Sudoku Studio.lnk' '$INSTDIR\\\\bin\\\\sudoku_ui.exe' '' '$INSTDIR\\\\docs\\\\icon.ico'"
  "CreateShortCut '$SMPROGRAMS\\\\Sudoku Studio\\\\Uninstall Sudoku Studio.lnk' '$INSTDIR\\\\Uninstall.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
  "Delete '$SMPROGRAMS\\\\Sudoku Studio\\\\Sudoku Studio.lnk'"
  "Delete '$SMPROGRAMS\\\\Sudoku Studio\\\\Uninstall Sudoku Studio.lnk'"
  "RMDir '$SMPROGRAMS\\\\Sudoku Studio'"
)

# Install rules for help files
install(DIRECTORY docs/help/ DESTINATION docs/help)
install(TARGETS sudoku_ui DESTINATION bin)

include(CPack)
