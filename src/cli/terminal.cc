#include <cli/terminal.h>
#include <cstdlib> // 用于 system 函数
#include <iostream>
#include <string>

#ifdef _WIN32
#include <conio.h>   // 用于 Windows 的 _kbhit
#include <windows.h> // 用于 Windows 系统
#endif

namespace lansend::cli {

#ifdef _WIN32
// Windows 系统下的实现
Terminal::Terminal() {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hIn, &mode);
    SetConsoleMode(hIn, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
}

Terminal::~Terminal() {
    SetConsoleMode(hIn, mode);
}

void Terminal::ClearScreen() {
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    GetConsoleScreenBufferInfo(hOut, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    FillConsoleOutputCharacter(hOut, (TCHAR) ' ', dwConSize, coordScreen, &cCharsWritten);
    GetConsoleScreenBufferInfo(hOut, &csbi);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hOut, coordScreen);
}

void Terminal::SetCursorPosition(int x, int y) {
    COORD coord;
    coord.X = static_cast<SHORT>(x);
    coord.Y = static_cast<SHORT>(y);
    SetConsoleCursorPosition(hOut, coord);
}

std::string Terminal::ReadLine() {
    std::string line;
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b') {
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
        } else {
            line += ch;
            std::cout << ch;
            std::cout.flush();
        }
    }
    std::cout << std::endl;
    return line;
}

bool Terminal::IsKeyPressed() {
    return _kbhit();
}
#else
// POSIX 系统下的实现
#include <termios.h>
#include <unistd.h>

Terminal::Terminal() {
    tcgetattr(STDIN_FILENO, &oldt_);
    newt_ = oldt_;
    newt_.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt_);
}

Terminal::~Terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt_);
}

void Terminal::ClearScreen() {
    std::system("clear");
}

void Terminal::SetCursorPosition(int x, int y) {
    std::cout << "\033[" << y + 1 << ";" << x + 1 << "H";
    std::cout.flush();
}

std::string Terminal::ReadLine() {
    std::string line;
    char ch;
    while ((ch = getchar()) != '\n') {
        if (ch == '\b') {
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
        } else {
            line += ch;
            std::cout << ch;
            std::cout.flush();
        }
    }
    std::cout << std::endl;
    return line;
}

bool Terminal::IsKeyPressed() {
    fd_set set;
    struct timeval timeout;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout) > 0;
}
#endif

void Terminal::PrintInfo(const std::string& message) {
#ifdef _WIN32
    SetConsoleTextAttribute(hOut, FOREGROUND_GREEN);
    std::cout << "[INFO] " << message << std::endl;
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    std::cout << "\033[32m[INFO] " << message << "\033[0m" << std::endl;
#endif
}

void Terminal::PrintError(const std::string& message) {
#ifdef _WIN32
    SetConsoleTextAttribute(hOut, FOREGROUND_RED);
    std::cerr << "[ERROR] " << message << std::endl;
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    std::cerr << "\033[31m[ERROR] " << message << "\033[0m" << std::endl;
#endif
}

void Terminal::PrintPrompt() {
    std::cout << "> ";
    std::cout.flush();
}

} // namespace lansend::cli
