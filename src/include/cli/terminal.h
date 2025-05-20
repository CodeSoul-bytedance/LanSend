#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#endif

class Terminal {
public:
    Terminal();
    ~Terminal();

    void clear_screen();
    void set_cursor_position(int x, int y);
    std::string read_line();
    bool is_key_pressed();

    void print_info(const std::string& message);
    void print_error(const std::string& message);
    void print_prompt();

private:
    void init_terminal();
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