#pragma once

#include <string>

class Terminal {
public:
    Terminal();
    ~Terminal();

    void clear_screen();
    void set_cursor_position(int x, int y);
    std::string read_line();
    bool is_key_pressed();

private:
    void init_terminal();
    void restore_terminal();
};