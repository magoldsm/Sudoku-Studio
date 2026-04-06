#ifdef _WIN32
#include <windows.h>

// Forward declaration of main
int main(int argc, char** argv);

// WinMain entry point for /SUBSYSTEM:WINDOWS (hides console)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  return main(__argc, __argv);
}

#endif
