#include "platform.h"

#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>


#define SAVE_FILE               "ConsoleTetris.dat"

// 每个骨板有4个方块，最长/宽也就是4
#define BLOCKS_PER_TETRIMINO    4
#define MAX_TETRIMINO_LENGTH    BLOCKS_PER_TETRIMINO

// 每行可以行动15次，到达次数后下降一行
#define FRAME_PER_ROW           15
// 2个预报
#define FORECAST_COUNT          2

// 游戏池顶部额外可见行数
#define EXTRA_VISIBLE           2
// 墙壁厚度
#define WALL_THICKNESS          1
// 游戏池外边距
#define WELL_MARGIN             1
// 右侧信息面板外边距
#define PANEL_MARGIN            2


// 单个骨板的数据结构，包括其种类和各个方块的坐标
typedef struct {
    BlockType type;
    Coordinate x[BLOCKS_PER_TETRIMINO];
    Coordinate y[BLOCKS_PER_TETRIMINO];
} Tetrimino;


// 不同形状的骨板
const Tetrimino initial_tetriminos[] = {
        {BLOCK_TYPE_NULL, {0, 0, 0, 0}, {3, 2, 1, 0}}, // I
        {BLOCK_TYPE_NULL, {0, 1, 1, 0}, {1, 1, 0, 0}}, // O
        {BLOCK_TYPE_NULL, {0, 1, 2, 1}, {1, 1, 1, 0}}, // T
        {BLOCK_TYPE_NULL, {1, 1, 1, 0}, {2, 1, 0, 0}}, // J
        {BLOCK_TYPE_NULL, {0, 0, 0, 1}, {2, 1, 0, 0}}, // L
        {BLOCK_TYPE_NULL, {2, 1, 1, 0}, {1, 1, 0, 0}}, // S
        {BLOCK_TYPE_NULL, {0, 1, 1, 2}, {1, 1, 0, 0}}  // Z
};


// 保存游戏全部信息的结构体，可以用来保存恢复进度
typedef struct {
    Tetrimino previous;
    Tetrimino current;
    Tetrimino forecasts[FORECAST_COUNT + 1];
    uint32_t scores;
    uint32_t count;
    Coordinate width;
    Coordinate height;
    BlockType well[];
} GameInfo;


// 利用坐标获取游戏池的方块了类型，以左下第一个非墙壁方块为坐标原点，右上为正
static inline BlockType *well_block(GameInfo *game, Coordinate x, Coordinate y) {
    return &game->well[(y + WALL_THICKNESS) * (game->width + 2 * WALL_THICKNESS)
                       + x + WALL_THICKNESS];
}


// 计算游戏池某一行有多少方块
Coordinate count_row(GameInfo *game, Coordinate y) {
    Coordinate count = 0;
    for (Coordinate x = 0; x < game->width; x++) {
        count += *well_block(game, x, y) != BLOCK_TYPE_NULL;
    }
    return count;
}


// 设置输出光标位置，注意其与set_cursor_absolute_position的不同
// 前者以游戏池左下第一个非墙壁方块为坐标原点，后者以控制台左上角为坐标原点
// 前者右上为坐标正方向，后者右下
// 前者以2个字符为1个单位坐标（因为方块是2字符宽度），后者1字符1坐标
static inline void set_cursor(GameInfo *game, Coordinate x, Coordinate y) {
    set_cursor_absolute_position(
            2 * (x + WALL_THICKNESS + WELL_MARGIN),
            game->height + EXTRA_VISIBLE - 1 - y);
}


// 在右侧信息面板的某一行输出信息文本，格式化输出
static inline void printf_at_info_panel(GameInfo *game, Coordinate line, const char *format, ...) {
    set_cursor(game, game->width + WALL_THICKNESS + WELL_MARGIN + PANEL_MARGIN, line);
    clear_color();
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// 同上，但进行非格式化输出
static inline void print_at_info_panel(GameInfo *game, Coordinate line, const wchar_t *info) {
    printf_at_info_panel(game, line, "%ls", info);
}

// 输出一条提示消息并暂停程序，按任意键后继续
void alert_message(GameInfo *game, const wchar_t *info) {
    printf_at_info_panel(game, 18, "%.40ls", info);
    while (get_action(100) == ACTION_EMPTY);
    printf_at_info_panel(game, 18, "%-40ls", L"");
}


// 绘制/擦除单个骨板，利用offset_*可以实现在游戏池或者预报区域进行绘制
void draw_single_tetrimino(GameInfo *game, Tetrimino *tetrimino, bool positive,
                           Coordinate offset_x, Coordinate offset_y) {
    for (int i = 0; i < BLOCKS_PER_TETRIMINO; i++) {
        if (tetrimino->y[i] + offset_y < game->height + EXTRA_VISIBLE) {
            set_cursor(game, tetrimino->x[i] + offset_x, tetrimino->y[i] + offset_y);
            print_block(positive ? tetrimino->type : BLOCK_TYPE_NULL);
        }
    }
}


// 当新的骨板产生后，会要重绘右侧信息区域，包括预报和得分等
void redraw_info_panel(GameInfo *game) {
    Coordinate offset_x = game->width + WALL_THICKNESS + WELL_MARGIN + PANEL_MARGIN;
    Coordinate offset_y = 12;
    for (Coordinate f = 0; f < FORECAST_COUNT; f++) {
        Coordinate this_offset_x = offset_x + f * (MAX_TETRIMINO_LENGTH + 1);
        draw_single_tetrimino(game, &game->forecasts[f], false, this_offset_x, offset_y);
        draw_single_tetrimino(game, &game->forecasts[f + 1], true, this_offset_x, offset_y);
    }

    printf_at_info_panel(game, 10, "%ls: \t%"PRIu32, L"得分", game->scores);
    printf_at_info_panel(game, 9, "%ls: \t%"PRIu32, L"数量", game->count);
}


// 重绘游戏池中的全部方块
void redraw_well(GameInfo *game) {
    for (Coordinate y = -WALL_THICKNESS; y < game->height + EXTRA_VISIBLE; y++) {
        set_cursor(game, -WALL_THICKNESS, y);
        for (Coordinate x = -WALL_THICKNESS; x < game->width + WALL_THICKNESS; x++) {
            print_block(y >= game->height ? BLOCK_TYPE_NULL : *well_block(game, x, y));
        }
    }
}


// 初始化显示，清屏并输出一些固定文字
void init_display(GameInfo *game) {
    clear_screen();
    print_at_info_panel(game, 6, L"WSAD/方向键 旋转平移");
    print_at_info_panel(game, 5, L"空格/回车 快速下降");
    print_at_info_panel(game, 4, L"ctrl+P 暂停");
    print_at_info_panel(game, 3, L"ctrl+W 保存进度");
    print_at_info_panel(game, 2, L"ctrl+R 载入进度");
    print_at_info_panel(game, 1, L"ctrl+N 重新开始");
    print_at_info_panel(game, 0, L"ctrl+C 退出");
    // 很奇怪，这里不fflush会导致MAC下显示异常
    fflush(stdout);
    redraw_well(game);
}


// 在指定方向轴以指定偏移量平移一个骨板，如果没“碰壁”返回true，否则false
bool shift_tetrimino(GameInfo *game, Tetrimino *tetrimino, Coordinate *axis, Coordinate offset) {
    for (int i = 0; i < BLOCKS_PER_TETRIMINO; i++) {
        axis[i] += offset;
        Coordinate x = tetrimino->x[i];
        Coordinate y = tetrimino->y[i];
        if (game && *well_block(game, x, y) != BLOCK_TYPE_NULL) {
            do {
                axis[i] -= offset;
            } while (i-- > 0);
            return false;
        }
    }
    return true;
}


// 逆时旋转骨板，同理返回布尔值表示是否可行
bool rotate_tetrimino(GameInfo *game, Tetrimino *tetrimino) {
    // 获取左上下三个边界的位置    
    Coordinate left = tetrimino->x[0];
    Coordinate bottom = tetrimino->y[0];
    Coordinate top = tetrimino->y[0];
    for (int i = 1; i < BLOCKS_PER_TETRIMINO; i++) {
        left = tetrimino->x[i] < left ? tetrimino->x[i] : left;
        bottom = tetrimino->y[i] < bottom ? tetrimino->y[i] : bottom;
        top = tetrimino->y[i] > top ? tetrimino->y[i] : top;
    }

    // 旋转
    Tetrimino tmp;
    tmp.type = tetrimino->type;
    for (int i = 0; i < BLOCKS_PER_TETRIMINO; i++) {
        tmp.x[i] = left + (top - tetrimino->y[i]);
        tmp.y[i] = bottom + (tetrimino->x[i] - left);
    }

    // 如果旋转后需要往左或者往下平移可以避免碰壁，这也是允许的，通俗地说就是“顶过去”
    for (int i = 0; i < BLOCKS_PER_TETRIMINO; i++) {
        if (!game ||
            shift_tetrimino(game, &tmp, tmp.x, -i) ||
            shift_tetrimino(game, &tmp, tmp.y, -i)) {
            *tetrimino = tmp;
            return true;
        }
    }
    return false;
}


// 产生一个新的骨板，从预报依次递补
// 注意forecasts[0]始终与游戏池当前骨板一致，已经不是“预”报了，所以这个它其实是不会显示的
void generate_new_tetrimino(GameInfo *game) {
    memmove(&game->forecasts[0], &game->forecasts[1], sizeof(game->forecasts[0]) * FORECAST_COUNT);
    BlockType choice = rand() % (sizeof(initial_tetriminos) / sizeof(initial_tetriminos[0]));
    game->forecasts[FORECAST_COUNT] = initial_tetriminos[choice];
    game->forecasts[FORECAST_COUNT].type = BLOCK_TYPE_NORMAL_MIN + choice;
    for (int i = rand() % 4; i-- > 0;) {
        rotate_tetrimino(NULL, &game->forecasts[FORECAST_COUNT]);
    }
    game->current = game->forecasts[0];
    shift_tetrimino(NULL, &game->current, game->current.x, (game->width - MAX_TETRIMINO_LENGTH) / 2);
    shift_tetrimino(NULL, &game->current, game->current.y, game->height);
    game->previous = game->current;
}


// 保存进度
bool save_game(GameInfo *game) {
    bool successful = false;
    FILE *fp = fopen(SAVE_FILE, "w");
    if (fp) {
        uint32_t size = sizeof(GameInfo) + sizeof(BlockType) *
                                           (game->width + 2 * WALL_THICKNESS) *
                                           (game->height + MAX_TETRIMINO_LENGTH + WALL_THICKNESS);
        successful = fwrite(&size, sizeof(uint32_t), 1, fp) && fwrite(game, size, 1, fp);
        fclose(fp);
    }
    return successful;
}


// 载入进度
GameInfo *load_game(void) {
    GameInfo *game = NULL;
    FILE *fp = fopen(SAVE_FILE, "r");
    if (fp) {
        uint32_t size;
        if (fread(&size, sizeof(uint32_t), 1, fp)) {
            if (!(game = malloc(size)) || !fread(game, size, 1, fp)) {
                free(game);
            }
        }
        fclose(fp);
    }
    return game;
}


// 初始化新游戏
GameInfo *create_new_game(void) {
    GameInfo *game;
    Coordinate width = 10;
    Coordinate height = 20;
    size_t well_size = sizeof(BlockType) *
                       (width + WALL_THICKNESS * 2) *
                       (height + WALL_THICKNESS + MAX_TETRIMINO_LENGTH);

    game = malloc(sizeof(GameInfo) + well_size);
    game->scores = game->count = 0;
    game->width = width;
    game->height = height;
    memset(game->well, BLOCK_TYPE_NULL, well_size);

    // 游戏池墙壁绘制
    for (Coordinate x = -WALL_THICKNESS; x < game->width + WALL_THICKNESS; x++) {
        if (x < 0 || x >= game->width) {
            for (Coordinate y = 0; y < game->height + MAX_TETRIMINO_LENGTH; y++) {
                *well_block(game, x, y) = BLOCK_TYPE_WALL;
            }
        }
        for (Coordinate y = 1; y <= WALL_THICKNESS; y++) {
            *well_block(game, x, -y) = BLOCK_TYPE_WALL;
        }
    }

    // 初始产生几个骨板，填满预报
    for (int i = 0; i <= FORECAST_COUNT; i++) {
        generate_new_tetrimino(game);
    }
    return game;
}


// 进行游戏
GameInfo *start_game(GameInfo *game) {
    init_display(game);
    // 每一次循环表示生成了一个新骨板进入了游戏池
    while (true) {
        // 放在判断语句前，可以保证redraw_info_panel被调用
        redraw_info_panel(game);
        // 判断是否游戏结束
        if (count_row(game, game->height)) {
            alert_message(game, L"GAME OVER，按任意键退出");
            return NULL;
        }
        draw_single_tetrimino(game, &game->current, true, 0, 0);

        // 每一次循环代表一帧，每次可以行动一次，到达次数上限后会自动下落一行
        for (int frame = 0; frame <= FRAME_PER_ROW; frame++) {
            // 分数越大，时间越短
            Action action = frame == FRAME_PER_ROW ? ACTION_DOWN :
                            get_action(4000 / (100 + game->scores) + 30);
            bool tetrimino_moved = false;
            GameInfo *loaded_game = NULL;
            switch (action) {
                case ACTION_LEFT:
                    tetrimino_moved = shift_tetrimino(game, &game->current, game->current.x, -1);
                    break;
                case ACTION_RIGHT:
                    tetrimino_moved = shift_tetrimino(game, &game->current, game->current.x, 1);
                    break;
                case ACTION_ROTATE:
                    tetrimino_moved = rotate_tetrimino(game, &game->current);
                    break;
                case ACTION_DOWN:
                    tetrimino_moved = shift_tetrimino(game, &game->current, game->current.y, -1);
                    frame = tetrimino_moved ? 0 : FRAME_PER_ROW;
                    break;
                case ACTION_FAST_DOWN:
                    while (shift_tetrimino(game, &game->current, game->current.y, -1)) {
                        tetrimino_moved = true;
                    }
                    frame = tetrimino_moved ? 0 : FRAME_PER_ROW;
                    break;
                case ACTION_PAUSE:
                    alert_message(game, L"游戏已暂停，按任意键继续");
                    break;
                case ACTION_SAVE:
                    alert_message(game, save_game(game) ?
                                        L"保存进度成功，按任意键继续" :
                                        L"保存进度失败，按任意键继续");
                    break;
                case ACTION_LOAD:
                    if ((loaded_game = load_game())) {
                        alert_message(game, L"载入进度成功，按任意键开始");
                        return loaded_game;
                    } else {
                        alert_message(game, L"载入进度失败，按任意键开始");
                    }
                    break;
                case ACTION_NEW_GAME:
                    return create_new_game();
                default:
                    break;
            }

            // 如果移动了，重绘当前活动骨板，采取差量重绘法，先擦除旧的，在绘制新的，更加高效
            if (tetrimino_moved) {
                draw_single_tetrimino(game, &game->previous, false, 0, 0);
                draw_single_tetrimino(game, &game->current, true, 0, 0);
                game->previous = game->current;
            }
        }

        // 骨板坠地，置入游戏池
        for (int i = 0; i < BLOCKS_PER_TETRIMINO; i++) {
            *well_block(game, game->current.x[i], game->current.y[i]) = game->current.type;
        }

        // 消行可得分
        int full_count = 0;
        for (Coordinate y = 0; y < game->height; y++) {
            if (count_row(game, y) == game->width) {
                full_count++;
                memmove(well_block(game, -WALL_THICKNESS, y), well_block(game, -WALL_THICKNESS, y + 1),
                        (game->width + 2 * WALL_THICKNESS) * (game->height + MAX_TETRIMINO_LENGTH - 1 - y));
                memset(well_block(game, 0, game->height + MAX_TETRIMINO_LENGTH - 1),
                       BLOCK_TYPE_NULL, game->width);
                y--;
            }
        }
        if (full_count) {
            redraw_well(game);
            game->scores += full_count * (full_count + 1) / 2;
        }

        // 产生新的骨板进入下一个循环
        game->count++;
        generate_new_tetrimino(game);
    }
}


// 被杀死前，先恢复控制台
void signal_kill(int sig) {
    restore_console();
    exit(0);
}


int main(void) {
    setlocale(LC_CTYPE, "");
    // 准备控制台
    prepare_console();
    // 信号处理
    signal(SIGABRT, signal_kill);
    signal(SIGINT, signal_kill);
    signal(SIGTERM, signal_kill);

    srand((unsigned) time(NULL));

    GameInfo *game = load_game();
    if (!game) {
        game = create_new_game();
    }
    // start_game返回NULL表示退出，返回GameInfo *表示载入该结构体中的游戏
    while (game) {
        GameInfo *g = start_game(game);
        free(game);
        game = g;
    }

    restore_console();
    return 0;
}
