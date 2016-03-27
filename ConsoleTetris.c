#if (defined unix) || (defined __unix) || (defined __unix__)
#define ON_UNIX
#elif (defined WIN32) || (defined _WIN32) || (defined __WIN32) || (defined __WIN32__)
#define ON_WINDOWS
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#else
#error It can only be compiled on Unix-like or Windows platform!
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined ON_UNIX
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined ON_WINDOWS
#include <conio.h>
#include <windows.h>
#endif

#define DO_LEFT 0
#define DO_RIGHT 1
#define DO_RORATE 2
#define DO_DOWN 3
#define DO_PAUSE 4
#define DO_WRITE 5
#define DO_READ 6
#define DO_OTHER 7
#define DO_NOTHING 8

#if defined ON_UNIX
#define TO_CONSOLE_AT(x, y) {printf("\033[%d;%dH", y + 1, x + 1);}

#define SET_COLOR(color)\
if (color >= 1 && color <= 7) {\
    printf("\033[0;%d;1m", color + 30);\
} else {\
    printf("\033[0;37m");\
}

#elif defined ON_WINDOWS
#define TO_CONSOLE_AT(x, y) {coord.X = x; coord.Y = y; SetConsoleCursorPosition(handle, coord);}

#define SET_COLOR(color)\
if (color >= 1 && color <= 7) {\
    SetConsoleTextAttribute(handle, color | FOREGROUND_INTENSITY);\
} else {\
    SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);\
}
#endif

#define PRINTF_AT(x, y, ...) {TO_CONSOLE_AT(x, y); printf(__VA_ARGS__);}

#define TO_POOL_AT(x, y) {TO_CONSOLE_AT(2 * x + 2, 21 - y);}

#define PRINT_BLOCK(type) {SET_COLOR(type); printf(type == 0 ? "  " : type == 8 ? "乂" : "囗");}

#define MAX(a, b, c, d) ((a > b ? a : b) > (c > d ? c : d) ? (a > b ? a : b) : (c > d ? c : d))

#define MIN(a, b, c, d) ((a < b ? a : b) < (c < d ? c : d) ? (a < b ? a : b) : (c < d ? c : d))

struct Plate {
    char type;
    char x[4];
    char y[4];
};

struct {
    char pool[12][25];
    struct Plate previous;
    struct Plate current;
    struct Plate forecast;
    long mark;
    long count;
} game;

const struct Plate initialPlates[7] = {
    { 1, { 4, 4, 4, 4 }, { 21, 22, 23, 24 } },
    { 2, { 4, 4, 5, 5 }, { 21, 22, 22, 21 } },
    { 3, { 4, 4, 4, 5 }, { 21, 22, 23, 21 } },
    { 4, { 4, 5, 5, 5 }, { 21, 21, 22, 23 } },
    { 5, { 4, 5, 5, 6 }, { 21, 21, 22, 22 } },
    { 6, { 4, 5, 5, 6 }, { 22, 21, 22, 21 } },
    { 7, { 4, 5, 6, 5 }, { 22, 22, 22, 21 } }
};

#if defined ON_UNIX
static struct termios oldTermios;
static int oldFcntl;
#elif defined ON_WINDOWS
static COORD coord;
static HANDLE handle;
#endif

void PrepareConsole(void);
void RestoreConsole(void);
int InputCheck(void);
void RedrawPlate(void);
void RedrawConsole(void);
void ShowInfo(char *info);
int Move(struct Plate *origin, int direction);

#if defined ON_UNIX
void PrepareConsole(void)
{
    tcgetattr(STDIN_FILENO, &oldTermios);
    struct termios newTermios = oldTermios;
    newTermios.c_lflag &= ~(ECHO | ICANON);
    oldFcntl = fcntl(STDIN_FILENO, F_GETFL);
    int newFcntl = oldFcntl | O_NONBLOCK;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newTermios) == -1 || fcntl(STDIN_FILENO, F_SETFL, newFcntl) == -1) {
        RestoreConsole();
        exit(-1);
    }
    SET_COLOR(0);
    system("clear");
}

void RestoreConsole(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    fcntl(STDIN_FILENO, F_SETFL, oldFcntl);
}

int InputCheck(void)
{
    usleep(1000 * (7000 / (100 + game.mark) + 30));
    int p = EOF, q = EOF;
    while ((p = getchar()) != EOF) {
        q = p;
    }
    return q == EOF ? DO_NOTHING : q == 16 ? DO_PAUSE : q == 23 ? DO_WRITE : q == 18 ? DO_READ :
           q == 'a' ? DO_LEFT : q == 'd' ? DO_RIGHT : q == 'w' ? DO_RORATE : q == 's' ? DO_DOWN :
           q == 'A' ? DO_LEFT : q == 'D' ? DO_RIGHT : q == 'W' ? DO_RORATE : q == 'S' ? DO_DOWN : DO_OTHER;
}

#elif defined ON_WINDOWS
void PrepareConsole(void)
{
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SET_COLOR(0);
    system("cls");
}

void RestoreConsole(void)
{
    CloseHandle(handle);
}

int InputCheck(void)
{
    Sleep(7000 / (100 + game.mark) + 30);
    int q = EOF;
    while (_kbhit()) {
        q = _getch();
    }
    return q == EOF ? DO_NOTHING : q == 16 ? DO_PAUSE : q == 23 ? DO_WRITE : q == 18 ? DO_READ :
           q == 'a' ? DO_LEFT : q == 'd' ? DO_RIGHT : q == 'w' ? DO_RORATE : q == 's' ? DO_DOWN :
           q == 'A' ? DO_LEFT : q == 'D' ? DO_RIGHT : q == 'W' ? DO_RORATE : q == 'S' ? DO_DOWN : DO_OTHER;
}
#endif

void RedrawPlate(void)
{
    int i, x, y;
    for (i = 0; i < 4; i++) {
        x = game.previous.x[i];
        y = game.previous.y[i];
        if (x > 0 && x < 11 && y > 0 && y < 21) {
            TO_POOL_AT(x, y);
            PRINT_BLOCK(0);
        }
    }
    for (i = 0; i < 4; i++) {
        x = game.current.x[i];
        y = game.current.y[i];
        if (x > 0 && x < 11 && y > 0 && y < 21) {
            TO_POOL_AT(x, y);
            PRINT_BLOCK(game.current.type);
        }
    }
    game.previous = game.current;
    TO_CONSOLE_AT(0, 22);
}

void RedrawConsole(void)
{
    int i, j;
    for (j = 0; j < 21; j++) {
        TO_POOL_AT(0, j);
        for (i = 0; i < 12; i++) {
            PRINT_BLOCK(game.pool[i][j]);
        }
    }
    PRINTF_AT(40, 1, "下一个：");
    for (i = 2; i < 6; i++) {
        PRINTF_AT(40, i, "        ");
    }
    for (i = 0; i < 4; i++) {
        TO_CONSOLE_AT(2 * game.forecast.x[i] + 32, 26 - game.forecast.y[i]);
        PRINT_BLOCK(game.forecast.type);
    }
    SET_COLOR(0);
    PRINTF_AT(40, 9, "当前已得分\t%ld", game.mark);
    PRINTF_AT(40, 11, "当前板块数\t%ld", game.count);
    PRINTF_AT(40, 17, "WASD控制方块");
    PRINTF_AT(40, 18, "ctrl+P键暂停");
    PRINTF_AT(40, 19, "ctrl+W键保存进度");
    PRINTF_AT(40, 20, "ctrl+R键载入进度");
    TO_CONSOLE_AT(0, 22);
}

void ShowInfo(char *info)
{
    SET_COLOR(0);
    PRINTF_AT(40, 14, "%-30s", info);
    TO_CONSOLE_AT(0, 22);
    while (InputCheck() == DO_NOTHING) {}
    PRINTF_AT(40, 14, "%-30s", "");
    TO_CONSOLE_AT(0, 22);
}

int Move(struct Plate *origin, int direction)
{
    struct Plate tmp = *origin;
    int i;
    if (DO_RORATE == direction) {
        char xMin = MIN(tmp.x[0], tmp.x[1], tmp.x[2], tmp.x[3]);
        char yMin = MIN(tmp.y[0], tmp.y[1], tmp.y[2], tmp.y[3]);
        char yMax = MAX(tmp.y[0], tmp.y[1], tmp.y[2], tmp.y[3]);
        char outOfWall = xMin + yMax - yMin - 10;
        outOfWall = outOfWall > 0 ? outOfWall : 0;
        for (i = 0; i < 4; i++) {
            char newY = yMin - xMin + tmp.x[i];
            tmp.x[i] = xMin + yMax - tmp.y[i] - outOfWall;
            tmp.y[i] = newY;
        }
    } else if (DO_DOWN == direction) {
        for (i = 0; i < 4; i++) {
            tmp.y[i] -= 1;
        }
    } else if (DO_LEFT == direction || DO_RIGHT == direction) {
        for (i = 0; i < 4; i++) {
            tmp.x[i] += direction == DO_LEFT ? -1 : 1;
        }
    }
    for (i = 0; i < 4; i++) {
        if (game.pool[tmp.x[i]][tmp.y[i]]) {
            return 0;
        }
    }
    *origin = tmp;
    return 1;
}

int main()
{
    PrepareConsole();
    int i, j;
    for (i = 0; i < 12; i++) {
        for (j = 0; j < 25; j++) {
            game.pool[i][j] = i == 0 || i == 11 || j == 0 ? 8 : 0;
        }
    }
    srand((unsigned)time(NULL));
    game.forecast = initialPlates[rand() % 7];
    i = rand() % 4;
    while (i-- > 0) {
        Move(&game.forecast, DO_RORATE);
    }
    game.mark = game.count = 0;
    int kick = DO_NOTHING;
    while (1) {
        game.previous = game.current = game.forecast;
        game.forecast = initialPlates[rand() % 7];
        i = rand() % 4;
        while (i-- > 0) {
            Move(&game.forecast, DO_RORATE);
        }
        game.count++;
        RedrawConsole();
        RedrawPlate();
        int step = 0;
        for (;; step++) {
            kick = InputCheck();
            if (step >= 10 || DO_DOWN == kick) {
                if (Move(&game.current, DO_DOWN)) {
                    RedrawPlate();
                    step = 0;
                } else if (step >= 10) {
                    break;
                }
            } else if (DO_RORATE == kick || DO_LEFT == kick || DO_RIGHT == kick) {
                if (Move(&game.current, kick)) {
                    RedrawPlate();
                }
            } else if (DO_PAUSE == kick) {
                ShowInfo("游戏已暂停，按任意键继续");
            } else if (DO_WRITE == kick) {
                FILE *fp = fopen("ConsoleTetris.progress", "w");
                if (fp != NULL) {
                    fwrite(&game, sizeof(game), 1, fp);
                    fclose(fp);
                    ShowInfo("写入进度成功，按任意键继续");
                }
            } else if (DO_READ == kick) {
                FILE *fp = fp = fopen("ConsoleTetris.progress", "r");
                if (fp != NULL) {
                    fread(&game, sizeof(game), 1, fp);
                    fclose(fp);
                    RedrawConsole();
                    RedrawPlate();
                    ShowInfo("载入进度成功，按任意键继续");
                }
            }
        }

        for (i = 0; i < 4; i++) {
            game.pool[game.current.x[i]][game.current.y[i]] = game.current.type;
        }

        int fullRowCount = 0;
        for (j = 1; j < 21; j++) {
            int rowIsFull = 1;
            for (i = 1; i < 11 && rowIsFull; rowIsFull = game.pool[i++][j] > 0);
            if (rowIsFull) {
                fullRowCount++;
                int j2, i2;
                for (j2 = j + 1; j2 < 25; j2++) {
                    for (i2 = 1; i2 < 11; i2++) {
                        game.pool[i2][j2 - 1] = game.pool[i2][j2];
                    }
                }
                j--;
            }
        }
        if (fullRowCount) {
            game.mark += fullRowCount == 1 ? 1 : fullRowCount == 2 ? 3 : fullRowCount == 3 ? 6 : 10;
            RedrawConsole();
        }

        int isOverFlow = 0;
        for (i = 1; i < 11 && !isOverFlow; isOverFlow = game.pool[i++][21]);
        if (isOverFlow) {
            ShowInfo("GAME OVER，按任意键退出");
            break;
        }
    }
    RestoreConsole();
    return 0;
}