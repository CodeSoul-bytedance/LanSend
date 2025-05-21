#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#endif

namespace lansend::cli {

class Terminal {
public:
    Terminal();
    ~Terminal();

    void ClearScreen();
    void SetCursorPosition(int x, int y);
    std::string ReadLine();
    bool IsKeyPressed();

    void PrintInfo(const std::string& message);
    void PrintError(const std::string& message);
    void PrintPrompt();

private:
    void initTerminal();
    void restoreTerminal();

#ifdef _WIN32
    HANDLE hIn;
    HANDLE hOut;
    DWORD mode;
#else
    struct termios oldt_;
    struct termios newt_;
#endif
};

} // namespace lansend::cli