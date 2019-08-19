#include "platform.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#define ESC "\x1B["


static int old_fcntl;
static struct termios old_termios;
static struct winsize console_size;


void prepare_console(void) {
    // 无回显，不需要回车
    tcgetattr(STDIN_FILENO, &old_termios);
    struct termios t = old_termios;
    t.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    // 无阻塞
    old_fcntl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, old_fcntl | O_NONBLOCK);

    // 无光标
    printf(ESC"?25l");
    // POSIX清屏，这里的清屏和clear_screen作用不同，后者会填充空格来绘制背景颜色
    printf(ESC"2J");
    // 获取控制台大小
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &console_size);
}


void restore_console(void) {
    fcntl(STDIN_FILENO, F_SETFL, old_fcntl);
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf(ESC"?25h");
    // 到窗口左下角去，但要必须要输出一个换行才能恢复输出文本属性
    set_cursor_absolute_position(0, console_size.ws_row - 1);
    printf(ESC"0m\n");
}


void clear_screen(void) {
    // 光标置于左上角，黑底白字无高亮用空格回车刷满屏幕
    set_cursor_absolute_position(0, 0);
    printf(ESC"0;37;40m");
    for (int y = 0; y < console_size.ws_row; y++) {
        for (int x = 0; x < console_size.ws_col; x++) {
            printf(" ");
        }
        printf("\n");
    }
}


void print_block(BlockType type) {
    if (type == BLOCK_TYPE_NULL) {
        printf(ESC"0;37;40m  ");
    } else if (type == BLOCK_TYPE_WALL) {
        printf(ESC"0;37;40m%ls", L"囗");
    } else {
        printf(ESC"%d;40;1m%ls", type % 6 + 31, L"田");
    }
}

void clear_color(void) {
    printf(ESC"0;37;40m");
}

void set_cursor_absolute_position(Coordinate x, Coordinate y) {
    // POSIX控制台坐标从1开始
    printf(ESC"%d;%dH", y + 1, x + 1);
}


Action get_action(uint32_t wait_time) {
    usleep(1000 * wait_time);

    static int buffer[3];
    size_t read_count = 0;
    int ch;
    while ((ch = getchar()) != EOF) {
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
            case '\n':
                return ACTION_FAST_DOWN;
            case 0x1B:
                if (read_count >= 3 && buffer[1] == '[') {
                    switch (buffer[2]) {
                        case 'A':
                            return ACTION_ROTATE;
                        case 'B':
                            return ACTION_DOWN;
                        case 'C':
                            return ACTION_RIGHT;
                        case 'D':
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
