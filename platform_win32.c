#include "platform.h"

#include <conio.h>
#include <windows.h>


#define DEFAULT_COLOR (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)


static HANDLE handle;
static UINT old_cp;
static CONSOLE_CURSOR_INFO old_cursor_info;
static CONSOLE_SCREEN_BUFFER_INFO old_console_info;


void prepare_console(void) {
    handle = GetStdHandle(STD_OUTPUT_HANDLE);

    // 禁用光标
    GetConsoleCursorInfo(handle, &old_cursor_info);
    CONSOLE_CURSOR_INFO info = old_cursor_info;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(handle, &info);

    // 获取控制台有关信息
    GetConsoleScreenBufferInfo(handle, &old_console_info);
}


void restore_console(void) {
    // 恢复光标以及输出文本属性
    SetConsoleCursorInfo(handle, &old_cursor_info);
    SetConsoleTextAttribute(handle, old_console_info.wAttributes);
    // 到窗口左下角去
    set_cursor_absolute_position(0, old_console_info.srWindow.Bottom - old_console_info.srWindow.Top + 1);
    CloseHandle(handle);
}


void clear_screen(void) {
    // 用空格填满屏幕，Windows下不要回车效果好
    set_cursor_absolute_position(0, 0);
    SetConsoleTextAttribute(handle, DEFAULT_COLOR);
    for (int y = old_console_info.srWindow.Top; y <= old_console_info.srWindow.Bottom; y++) {
        for (int x = old_console_info.srWindow.Left; x <= old_console_info.srWindow.Right; x++) {
            printf(" ");
        }
    }
}


void print_block(BlockType type) {
    if (type == BLOCK_TYPE_NULL) {
        SetConsoleTextAttribute(handle, DEFAULT_COLOR);
        printf("  ");
    } else if (type == BLOCK_TYPE_WALL) {
        SetConsoleTextAttribute(handle, DEFAULT_COLOR);
        printf("::");
    } else {
        SetConsoleTextAttribute(handle, (type % 6 + 1) | FOREGROUND_INTENSITY);
        printf("▇");
    }
}


void print_text(const char *format, ...) {
    SetConsoleTextAttribute(handle, DEFAULT_COLOR);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}


void set_cursor_absolute_position(Coordinate x, Coordinate y) {
    // 窗口最左侧为X轴零点，但以光标初始高度为Y轴零点
    COORD coord = {x + old_console_info.srWindow.Left, y + old_console_info.dwCursorPosition.Y};
    SetConsoleCursorPosition(handle, coord);
}


Action get_action(uint32_t wait_time) {
    Sleep(wait_time);

    static int buffer[2];
    size_t read_count = 0;
    while (_kbhit()) {
        int ch = _getch();
        if (read_count < sizeof(buffer) / sizeof(buffer[0])) {
            buffer[read_count++] = ch;
        }
    }

    if (read_count == 0) {
        return ACTION_EMPTY;
    } else {
        switch (buffer[0]) {
            case 'P' - 64:
                return ACTION_PAUSE;
            case 'W' - 64:
                return ACTION_SAVE;
            case 'R' - 64:
                return ACTION_LOAD;
            case 'N' - 64:
                return ACTION_NEW_GAME;
            case 'w':
            case 'W':
                return ACTION_ROTATE;
            case 's':
            case 'S':
                return ACTION_DOWN;
            case 'a':
            case 'A':
                return ACTION_LEFT;
            case 'd':
            case 'D':
                return ACTION_RIGHT;
            case ' ':
            case '\r':
                return ACTION_FAST_DOWN;
            case 0xE0:
                if (read_count >= 2) {
                    switch (buffer[1]) {
                        case 'H':
                            return ACTION_ROTATE;
                        case 'P':
                            return ACTION_DOWN;
                        case 'M':
                            return ACTION_RIGHT;
                        case 'K':
                            return ACTION_LEFT;
                        default:
                            return ACTION_UNRECOGNIZED;
                    }
                }
                return ACTION_UNRECOGNIZED;
            default:
                return ACTION_UNRECOGNIZED;
        }
    }
}
