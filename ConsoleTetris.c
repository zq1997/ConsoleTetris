#if defined(__unix__) || defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_UNIX
#elif defined(_WIN32)
#define PLATFORM_WINDOWS
#else
#error It can only be compiled on Unix-like or Windows platform!
#endif


#if defined PLATFORM_WINDOWS
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif


#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined PLATFORM_UNIX

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#elif defined PLATFORM_WINDOWS

#include <conio.h>
#include <windows.h>

#endif


// 各种游戏操作的宏定义：方块控制，暂停、保存、读取，以及无效操作
#define ACTION_NOTHING 0
#define ACTION_LEFT 1
#define ACTION_RIGHT 2
#define ACTION_ROTATE 3
#define ACTION_DOWN 4
#define ACTION_FAST_DOWN 5
#define ACTION_PAUSE 6
#define ACTION_WRITE 7
#define ACTION_READ 8
#define ACTION_NOT_VALID 9


// 在屏幕指定位置打印
#define PRINTF_AT(x, y, ...) do {\
        SetCursorPosition(x, y);\
        printf(__VA_ARGS__);\
    } while(0)

// 在游戏池指定位置打印
#define TO_POOL_AT(x, y) SetCursorPosition(2 * x + 2, 21 - y)

// 在光标位置输出一个空白/方块/墙（由type决定）
#define PRINT_BLOCK(type) do {\
        SetColor(type);\
        printf(type == 0 ? "  " : type == 8 ? "乂" : "囗");\
    } while(0)


// 每个俄罗斯方块的板块由四个方块构成（需要4对横纵坐标），总计7种类型（type）
// 特别地，用type=0表示空白区域，用type=8表示墙壁
struct Plate {
    char type;
    char x[4];
    char y[4];
};

// 游戏状态记录的结构体，包括游戏池，上一个、当前、下一个板块，得分，板块计数
struct {
    char pool[12][25];
    struct Plate previous;
    struct Plate current;
    struct Plate forecast;
    long mark;
    long count;
} game;


// 7种板块的初始状态，在游戏池的最高位置
const struct Plate PlatesGenerator[7] = {
        {1, {4, 4, 4, 4}, {21, 22, 23, 24}},
        {2, {4, 4, 5, 5}, {21, 22, 22, 21}},
        {3, {4, 4, 4, 5}, {21, 22, 23, 21}},
        {4, {4, 5, 5, 5}, {21, 21, 22, 23}},
        {5, {4, 5, 5, 6}, {21, 21, 22, 22}},
        {6, {4, 5, 5, 6}, {22, 21, 22, 21}},
        {7, {4, 5, 6, 5}, {22, 22, 22, 21}}
};


//这些函数涉及输入输出，其实现和操作系统有关
void PrepareConsole(void);

void RestoreConsole(void);

void SetCursorPosition(int x, int y);

void SetColor(int colorIndex);

int GetAction(void);

//这些函数只与游戏逻辑有关，统一实现
char Min4(const char *array);

char Max4(const char *array);

void RedrawPlate(void);

void RedrawConsole(void);

void ShowInfo(char *info);

int Move(struct Plate *origin, int action);

#if defined PLATFORM_UNIX
static struct termios oldTermios;
static int oldFcntl;

// Unix-like系统上，为了实现无阻塞输入需要有如下操作
void PrepareConsole(void) {
    tcgetattr(STDIN_FILENO, &oldTermios);
    struct termios newTermios = oldTermios;
    newTermios.c_lflag &= ~(ECHO | ICANON);
    oldFcntl = fcntl(STDIN_FILENO, F_GETFL);
    int newFcntl = oldFcntl | O_NONBLOCK;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newTermios) == -1 ||
        fcntl(STDIN_FILENO, F_SETFL, newFcntl) == -1) {
        RestoreConsole();
        exit(-1);
    }
    SetColor(0);
    system("clear");
}

// 关闭时，还原PrepareConsole中的各种设定
void RestoreConsole(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    fcntl(STDIN_FILENO, F_SETFL, oldFcntl);
}

// 设置光标位置
void SetCursorPosition(int x, int y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

// 设置后续printf输出的颜色，colorIndex只是代表不同颜色，不是RGB
void SetColor(int colorIndex) {
    if (colorIndex >= 1 && colorIndex <= 7) {
        printf("\033[0;%d;1m", colorIndex + 30);
    } else {
        printf("\033[0;37m");
    }
}

// 获取用户游戏输入，得分越高反应时间越短
int GetAction(void) {
    usleep(1000 * (7000 / (100 + game.mark) + 30));
    int c = EOF, ch = EOF;
    while ((c = getchar()) != EOF) {
        ch = c;
    }
    return ch == EOF ? ACTION_NOTHING :
           ch == 'a' || ch == 'A' ? ACTION_LEFT :
           ch == 'd' || ch == 'D' ? ACTION_RIGHT :
           ch == 'w' || ch == 'W' ? ACTION_ROTATE :
           ch == 's' || ch == 'S' ? ACTION_DOWN :
           ch == ' ' ? ACTION_FAST_DOWN :
           ch == 'P' - 64 ? ACTION_PAUSE :
           ch == 'W' - 64 ? ACTION_WRITE :
           ch == 'R' - 64 ? ACTION_READ :
           ACTION_NOT_VALID;
}

#elif defined PLATFORM_WINDOWS
static HANDLE handle;

// Windows上设置光标位置必须获取对应Handle
void PrepareConsole(void) {
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetColor(0);
    system("cls");
}

// 关闭句柄
void RestoreConsole(void) {
    CloseHandle(handle);
}

void SetCursorPosition(int x, int y) {
    COORD coord = {x, y};
    SetConsoleCursorPosition(handle, coord);
}

void SetColor(int colorIndex) {
    if (colorIndex >= 1 && colorIndex <= 7) {
        SetConsoleTextAttribute(handle, colorIndex | FOREGROUND_INTENSITY);
    } else {
        SetConsoleTextAttribute(handle,
                                FOREGROUND_RED | FOREGROUND_GREEN |
                                FOREGROUND_BLUE);
    }
}

int GetAction(void) {
    Sleep(7000 / (100 + game.mark) + 30);
    int ch = EOF;
    while (_kbhit()) {
        ch = _getch();
    }
    return ch == EOF ? ACTION_NOTHING :
           ch == 'a' || ch == 'A' ? ACTION_LEFT :
           ch == 'd' || ch == 'D' ? ACTION_RIGHT :
           ch == 'w' || ch == 'W' ? ACTION_ROTATE :
           ch == 's' || ch == 'S' ? ACTION_DOWN :
           ch == ' ' ? ACTION_FAST_DOWN :
           ch == 'P' - 64 ? ACTION_PAUSE :
           ch == 'W' - 64 ? ACTION_WRITE :
           ch == 'R' - 64 ? ACTION_READ :
           ACTION_NOT_VALID;
}

#endif

// 获取一个容量为4的char数组的最小值
char Min4(const char *array) {
    char m = array[0] < array[1] ? array[0] : array[1];
    char n = array[2] < array[3] ? array[2] : array[3];
    return m < n ? m : n;
}

// 获取一个容量为4的char数组的最大值
char Max4(const char *array) {
    char m = array[0] > array[1] ? array[0] : array[1];
    char n = array[2] > array[3] ? array[2] : array[3];
    return m > n ? m : n;
}

// 当板块移动后，需要在旧的位置上填上空白，并在新位置上画上板块
void RedrawPlate(void) {
    for (int i = 0; i < 4; i++) {
        int x = game.previous.x[i];
        int y = game.previous.y[i];
        if (x > 0 && x < 11 && y > 0 && y < 21) {
            TO_POOL_AT(x, y);
            PRINT_BLOCK(0);
        }
    }
    for (int i = 0; i < 4; i++) {
        int x = game.current.x[i];
        int y = game.current.y[i];
        if (x > 0 && x < 11 && y > 0 && y < 21) {
            TO_POOL_AT(x, y);
            PRINT_BLOCK(game.current.type);
        }
    }
    game.previous = game.current;
    SetCursorPosition(0, 22);
}

// 当消行、载入等情况后，需要重绘整个控制台，包括游戏池右侧信息输出区域等
void RedrawConsole(void) {
    for (int j = 0; j < 21; j++) {
        TO_POOL_AT(0, j);
        for (int i = 0; i < 12; i++) {
            PRINT_BLOCK(game.pool[i][j]);
        }
    }
    PRINTF_AT(40, 1, "下一个：");
    for (int i = 2; i < 6; i++) {
        PRINTF_AT(40, i, "        ");
    }
    for (int i = 0; i < 4; i++) {
        SetCursorPosition(2 * game.forecast.x[i] + 32, 26 - game.forecast.y[i]);
        PRINT_BLOCK(game.forecast.type);
    }
    SetColor(0);
    PRINTF_AT(40, 9, "当前已得分\t%ld", game.mark);
    PRINTF_AT(40, 11, "当前板块数\t%ld", game.count);
    PRINTF_AT(40, 16, "WASD 控制板块");
    PRINTF_AT(40, 17, "空格 速降");
    PRINTF_AT(40, 18, "ctrl+P 暂停");
    PRINTF_AT(40, 19, "ctrl+W 保存进度");
    PRINTF_AT(40, 20, "ctrl+R 载入进度");
    SetCursorPosition(0, 22);
}

// 输出一条信息，按任意键继续并消除该信息
void ShowInfo(char *info) {
    SetColor(0);
    PRINTF_AT(40, 14, "%-30s", info);
    SetCursorPosition(0, 22);
    while (GetAction() == ACTION_NOTHING);
    PRINTF_AT(40, 14, "%-30s", "");
    SetCursorPosition(0, 22);
}

// 将板块移动一个位置，成功返回非零并且改写原Plate结构体，失败返回0
int Move(struct Plate *origin, int action) {
    struct Plate tmp = *origin;
    if (ACTION_ROTATE == action) {
        char xMin = Min4(tmp.x);
        char yMin = Min4(tmp.y);
        char yMax = Max4(tmp.y);
        char outOfWall = xMin + yMax - yMin - 10;
        outOfWall = outOfWall > 0 ? outOfWall : 0;
        for (int i = 0; i < 4; i++) {
            char newY = yMin - xMin + tmp.x[i];
            tmp.x[i] = xMin + yMax - tmp.y[i] - outOfWall;
            tmp.y[i] = newY;
        }
    } else if (ACTION_DOWN == action) {
        for (int i = 0; i < 4; i++) {
            tmp.y[i] -= 1;
        }
    } else if (ACTION_LEFT == action || ACTION_RIGHT == action) {
        for (int i = 0; i < 4; i++) {
            tmp.x[i] += action == ACTION_LEFT ? -1 : 1;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (game.pool[tmp.x[i]][tmp.y[i]]) {
            return 0;
        }
    }
    *origin = tmp;
    return 1;
}

int main() {
    // 准备控制台
    PrepareConsole();

    game.mark = game.count = 0;
    // 准备墙壁和空白区域
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 25; j++) {
            game.pool[i][j] = i == 0 || i == 11 || j == 0 ? 8 : 0;
        }
    }

    // 初始生成一个板块并具有随机的方向
    srand((unsigned) time(NULL));
    game.forecast = PlatesGenerator[rand() % 7];
    for (int i = rand() % 4; i-- > 0;) {
        Move(&game.forecast, ACTION_ROTATE);
    }

    // 每一次循环表示生成了一个新板块进入游戏池
    while (1) {
        game.previous = game.current = game.forecast;
        game.forecast = PlatesGenerator[rand() % 7];
        for (int i = rand() % 4; i-- > 0;) {
            Move(&game.forecast, ACTION_ROTATE);
        }
        game.count++;
        RedrawConsole();
        RedrawPlate();

        // 每一次循环代表一步，每十步方块会自动下落一行
        for (int step = 0;; step++) {
            int action = GetAction();
            if (step >= 10) {
                action = ACTION_DOWN;
            }

            if (ACTION_NOT_VALID == action) {
                continue;
            } else if (ACTION_PAUSE == action) {
                ShowInfo("游戏已暂停，按任意键继续");
            } else if (ACTION_WRITE == action) {
                FILE *fp = fopen("ConsoleTetris.progress", "w");
                if (fp != NULL) {
                    fwrite(&game, sizeof(game), 1, fp);
                    fclose(fp);
                    ShowInfo("写入进度成功，按任意键继续");
                }
            } else if (ACTION_READ == action) {
                FILE *fp = fopen("ConsoleTetris.progress", "r");
                if (fp != NULL) {
                    fread(&game, sizeof(game), 1, fp);
                    fclose(fp);
                    RedrawConsole();
                    RedrawPlate();
                    ShowInfo("载入进度成功，按任意键继续");
                }
            } else if (ACTION_DOWN == action) {
                if (Move(&game.current, ACTION_DOWN)) {
                    RedrawPlate();
                    step = 0;
                } else {
                    break;
                }
            } else if (ACTION_FAST_DOWN == action) {
                int moved = 0;
                while (Move(&game.current, ACTION_DOWN)) {
                    moved = 1;
                }
                if (moved) {
                    RedrawPlate();
                    step = 0;
                } else {
                    break;
                }
            } else {
                if (Move(&game.current, action)) {
                    RedrawPlate();
                }
            }
        }

        // 板块坠地，固化置入游戏池
        for (int i = 0; i < 4; i++) {
            game.pool[game.current.x[i]][game.current.y[i]] = game.current.type;
        }

        // 消行并计数、加分
        int fullRowCount = 0;
        for (int j = 1; j < 21; j++) {
            int rowIsFull = 1;
            for (int i = 1; i < 11 && rowIsFull; i++) {
                rowIsFull = game.pool[i][j] > 0;
            }
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
            const static int MARKS[5] = {0, 1, 3, 6, 10};
            game.mark += MARKS[fullRowCount];
            RedrawConsole();
        }

        // 是否存在溢出，是的话游戏结束
        int isOverFlow = 0;
        for (int i = 1; i < 11 && !isOverFlow; i++) {
            isOverFlow = game.pool[i][21];
        }
        if (isOverFlow) {
            ShowInfo("GAME OVER，按任意键退出");
            break;
        }
    }

    // 恢复控制台
    RestoreConsole();
    return 0;
}
