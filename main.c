#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <limits.h>

#include "include/music.h"

/***********************************
 ** dos headers (fake on non-dos) **
 ***********************************/

#if (defined __BORLANDC__)
#  include <conio.h>  // kbhit()
#  include <dos.h>
#else

#  include "include/borland/conio.h"
#  include "include/borland/dos.h"

#  define _dos_getvect(ADDR_)           ((void *)0)
#  define _dos_setvect(ADDR_, HANDLER_) ((void)0)

#  define FP_SEG(FP_) (*(FP_ + 1))
#  define FP_OFF(FP_) (*FP_)

#  define _SP (0)
#  define _DS ((void *)0)
#  define _CS ((void *)0)
#endif

/******************
 ** conviniences **
 ******************/

#define elsizeof(ARR_) (sizeof(ARR_)[0])
#define lenof(ARR_)    (sizeof(ARR_) / elsizeof(ARR_))

#define xorswap(X_, Y_) (X_ ^= Y_, Y_ ^= X_, X_ ^= Y_)

/******************
 ** main program **
 ******************/

#define COLOR_BLOCK  ('\xDB')
#define COLOR_FIGURE ('\xDB')
#define COLOR_EMPTY  (' ')
#define COLOR_BG     ('\xFA')

#define FRAME_DELAY_MS        (83)
#define FRAMES_PER_LOGIC_TICK (5)

#define FIELD_WIDTH  (10)
#define FIELD_HEIGHT (20)

#define SCORE_STR_MAX_SIZE (6)

#define ROTATION_CHECK_ITER_LIMIT (16)

#define FIGURE_BLOCK_COUNT (4)

typedef struct V2 {
    int x, y;
} V2;
static V2 const figure_bounds = {4, 4};
static unsigned char const figure_types[][FIGURE_BLOCK_COUNT] = {
    {2, 6, 10, 14},  // I
    {1, 2, 5, 9},    // J
    {1, 2, 6, 10},   // L
    {5, 6, 9, 10},   // O
    {1, 5, 6, 10},   // S
    {2, 5, 6, 10},   // T
    {2, 5, 6, 9},    // Z
};

static char *field = NULL;

// score
static unsigned score = 0;
static char score_str[SCORE_STR_MAX_SIZE + 1] = {0};
static void update_score(void) {
    sprintf(score_str, "%0.*i", SCORE_STR_MAX_SIZE, score);
}

// figure
struct Figure {
    V2 pos;
    unsigned char type[FIGURE_BLOCK_COUNT];
} current_figure;

// generate
static unsigned next_figure_type_i;
void figure_generate(void) {
    unsigned j;

    current_figure.pos.x = FIELD_WIDTH / 2 - figure_bounds.x / 2;
    current_figure.pos.y = -1;

    for (j = 0; j < FIGURE_BLOCK_COUNT; j++)
        current_figure.type[j] = figure_types[next_figure_type_i][j];

    next_figure_type_i = rand() % lenof(figure_types);
}

// rotate
signed char rotate_dir;
V2 offset;
void figure_rotate(void) {
    unsigned i, j, check_iters;
    int x, y;
    int should_check;

    // rotate
    for (j = 0; j < FIGURE_BLOCK_COUNT; j++) {
        x = current_figure.type[j] % figure_bounds.x;
        y = current_figure.type[j] / figure_bounds.y;
        if (rotate_dir > 0) {
            y = figure_bounds.y - 1 - y;
            xorswap(x, y);
        } else {
            x = figure_bounds.x - 1 - x;
            xorswap(x, y);
        }

        current_figure.type[j] = x + figure_bounds.x * y;
    }

    // check if cannot be rotated
    offset.x = 0;
    offset.y = 0;
    check_iters = 0;
    do {
        should_check = 0;
        for (i = 0; i < FIGURE_BLOCK_COUNT; i++) {
            x = current_figure.pos.x + offset.x +
                current_figure.type[i] % figure_bounds.x;
            y = current_figure.pos.y + offset.y +
                current_figure.type[i] / figure_bounds.y;

            if (y < 0) {
                offset.y++;
                should_check = 1;
                break;
            } else if (y >= FIELD_HEIGHT) {
                offset.y--;
                should_check = 1;
                break;
            }

            if (x < 0) {
                offset.x++;
                should_check = 1;
                break;
            } else if (x >= FIELD_WIDTH) {
                offset.x--;
                should_check = 1;
                break;
            }

            if (field[x + y * FIELD_WIDTH] == COLOR_BLOCK)
                check_iters = ROTATION_CHECK_ITER_LIMIT;
        }

        check_iters++;
        if (check_iters >= ROTATION_CHECK_ITER_LIMIT || offset.x >= 2 ||
            offset.x <= -2 || offset.y <= -3) {
            rotate_dir = -rotate_dir;
            figure_rotate();
            return;
        }
    } while (should_check);

    current_figure.pos.x += offset.x;
    current_figure.pos.y += offset.y;
}

static V2 const field_render_pos = {3, 3},
                score_render_pos = {3 + FIELD_WIDTH + 2, 3},
                next_figure_render_pos = {3 + FIELD_WIDTH + 2, 6};

static struct text_info info = {0};
static char *screen = NULL;

static void screen_display(void) {
    static char far *const vmem = (char far *)0xB8000000;

    unsigned i;

    for (i = 0; i < info.screenwidth * info.screenheight; i++) {
        vmem[4 * i] = screen[i];
        vmem[4 * i + 2] =
            screen[i] == COLOR_BG + 1 || ' ' <= screen[i] && screen[i] <= '~'
                ? ' '
                : screen[i];
    }
}

void render_main(void) {
    unsigned i, j;

    memset(screen, COLOR_BG, info.screenwidth * info.screenheight);

    // render field
    for (i = 0; i < FIELD_WIDTH; i++)
        for (j = 0; j < FIELD_HEIGHT; j++) {
            screen
                [field_render_pos.x + i +
                 (j + field_render_pos.y) * info.screenwidth] =
                    field[i + j * FIELD_WIDTH];
        }

    // render figute
    for (i = 0; i < FIGURE_BLOCK_COUNT; i++) {
        screen
            [field_render_pos.x + current_figure.pos.x +
             current_figure.type[i] % figure_bounds.x +
             (current_figure.type[i] / figure_bounds.y + field_render_pos.y +
              current_figure.pos.y) *
                 info.screenwidth] = COLOR_FIGURE;
    }

    // render score
    for (i = 0; i < 5; i++) {
        screen[score_render_pos.x + i + score_render_pos.y * info.screenwidth] =
            "SCORE"[i];
    }
    for (i = 0; i < SCORE_STR_MAX_SIZE; i++) {
        screen
            [score_render_pos.x + i +
             (score_render_pos.y + 1) * info.screenwidth] = score_str[i];
    }

    // render next figure
    for (i = 0; i < 4; i++) {
        screen
            [next_figure_render_pos.x + i +
             next_figure_render_pos.y * info.screenwidth] = "NEXT"[i];
    }
    for (i = 0; i < figure_bounds.x; i++)
        for (j = 0; j < figure_bounds.y; j++) {
            screen
                [next_figure_render_pos.x + i +
                 (next_figure_render_pos.y + 1 + j) * info.screenwidth] =
                    COLOR_EMPTY;
        }
    for (i = 0; i < FIGURE_BLOCK_COUNT; i++) {
        screen
            [next_figure_render_pos.x +
             figure_types[next_figure_type_i][i] % figure_bounds.x +
             (figure_types[next_figure_type_i][i] / figure_bounds.y +
              next_figure_render_pos.y + 1) *
                 info.screenwidth] = COLOR_FIGURE;
    }

    screen_display();
}

static V2 const lose_render_pos = {
    3 + FIELD_WIDTH / 2 - 5,
    3 + FIELD_HEIGHT / 2 - 7
};
char const *lose_text =
    "          "
    " \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xBF "
    " \xB3 GAME \xB3 "
    " \xB3 OVER!\xB3 "
    " \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xD9 "
    "          "
    " \xDA\xC4\xC4\xC4\xC4\xC4\xC4\xBF "
    " \xB3 ESC- \xB3 "
    " \xB3 QUIT \xB3 "
    " \xB3      \xB3 "
    " \xB3  CR- \xB3 "
    " \xB3 AGAIN\xB3 "
    " \xC0\xC4\xC4\xC4\xC4\xC4\xC4\xD9 "
    "          ";
static V2 const lose_text_size = {10, 14};
void render_lose(void) {
    unsigned i, j;

    for (i = 0; i < lose_text_size.x; i++)
        for (j = 0; j < lose_text_size.y; j++)
            screen
                [lose_render_pos.x + i +
                 (lose_render_pos.y + j) * info.screenwidth] =
                    lose_text[i + j * lose_text_size.x];

    screen_display();
}

static MusicPlayer music;
static unsigned remaining_beep_duration_ms = 0;
static void beep_start(unsigned frequency, unsigned duration_ms) {
    remaining_beep_duration_ms = duration_ms;
    if (frequency) {
        frequency = 1193180l / frequency;

        outportb(0x43, 0xB6);

        outportb(0x42, (unsigned char)frequency),
            outportb(0x42, (unsigned char)(frequency >> 8));

        outportb(0x61, inportb(0x61) | 0x03);
    }
}
static void beep_stop(void) { outportb(0x61, inportb(0x61) & ~0x03); }
static void custom_delay(unsigned duration_ms) {
    delay(duration_ms);
    if (remaining_beep_duration_ms > duration_ms)
        remaining_beep_duration_ms -= duration_ms;
    else {
        beep_stop();
        music_player_next(&music);
    }
}

int main() {
    FILE *const file = fopen("tetris.mus", "r");

    srand(time(NULL));
    gettextinfo(&info);
    info.screenwidth /= 2;

    // init
    do screen = malloc(info.screenwidth * info.screenheight);
    while (screen == NULL);
    do field = malloc(FIELD_HEIGHT * FIELD_WIDTH);
    while (field == NULL);

restart:
    memset(field, COLOR_EMPTY, FIELD_HEIGHT * FIELD_WIDTH);

    update_score();

    music_player_init(&music, 166, beep_start);
    music_player_play(&music, file);

    // game
    {
        long frame;

        unsigned i, j;
        int do_collide = 0, is_on_edge = 0, did_fill_row;
        unsigned filled_rows_count;

        next_figure_type_i = rand() % lenof(figure_types);
        figure_generate();

        for (frame = 0; frame >= 0; frame++) {
            // input
            while (kbhit()) switch (getch()) {
                // quit
                case '\033': frame = LONG_MIN; break;

                // movement
                case 'a': {
                    is_on_edge = 0;
                    // check if can be moved
                    for (i = 0; i < FIGURE_BLOCK_COUNT; i++)
                        if (field
                                    [current_figure.pos.x - 1 +
                                     current_figure.type[i] % figure_bounds.x +
                                     (current_figure.type[i] / figure_bounds.y +
                                      current_figure.pos.y) *
                                         FIELD_WIDTH] == COLOR_BLOCK ||
                            field
                                    [current_figure.pos.x - 1 +
                                     current_figure.type[i] % figure_bounds.x +
                                     (current_figure.type[i] / figure_bounds.y +
                                      current_figure.pos.y + 1) *
                                         FIELD_WIDTH] == COLOR_BLOCK ||
                            current_figure.pos.x - 1 +
                                    current_figure.type[i] % figure_bounds.x <
                                0) {
                            is_on_edge = 1;
                            break;
                        }

                    if (!is_on_edge) current_figure.pos.x--;
                } break;

                case 'd': {
                    is_on_edge = 0;
                    // check if can be moved
                    for (i = 0; i < FIGURE_BLOCK_COUNT; i++)
                        if (field
                                    [current_figure.pos.x + 1 +
                                     current_figure.type[i] % figure_bounds.x +
                                     (current_figure.type[i] / figure_bounds.y +
                                      current_figure.pos.y) *
                                         FIELD_WIDTH] == COLOR_BLOCK ||
                            field
                                    [current_figure.pos.x + 1 +
                                     current_figure.type[i] % figure_bounds.x +
                                     (current_figure.type[i] / figure_bounds.y +
                                      current_figure.pos.y + 1) *
                                         FIELD_WIDTH] == COLOR_BLOCK ||
                            current_figure.pos.x + 1 +
                                    current_figure.type[i] % figure_bounds.x >=
                                FIELD_WIDTH) {
                            is_on_edge = 1;
                            break;
                        }

                    if (!is_on_edge) current_figure.pos.x++;
                } break;

                case 's': {
                    do_collide = 0;
                    // move down untill collision
                    while (!do_collide) {
                        custom_delay(FRAME_DELAY_MS / 2);
                        render_main();

                        current_figure.pos.y++;
                        for (i = 0; i < FIGURE_BLOCK_COUNT; i++)
                            if (field
                                        [current_figure.pos.x +
                                         current_figure.type[i] %
                                             figure_bounds.x +
                                         (current_figure.type[i] /
                                              figure_bounds.y +
                                          current_figure.pos.y) *
                                             FIELD_WIDTH] == COLOR_BLOCK ||
                                current_figure.type[i] / figure_bounds.y +
                                        current_figure.pos.y >=
                                    FIELD_HEIGHT) {
                                do_collide = 1;
                                current_figure.pos.y--;
                                break;
                            }
                    }

                    // skip to nearest logic frame
                    frame = (frame / FRAMES_PER_LOGIC_TICK + 1) *
                            FRAMES_PER_LOGIC_TICK;
                } break;

                // rotation
                case 'q': rotate_dir = -1, figure_rotate(); break;
                case 'e': rotate_dir = 1, figure_rotate(); break;
                }

            if (frame % FRAMES_PER_LOGIC_TICK == 0) {
                // collisions
                do_collide = 0;
                for (i = 0; i < FIGURE_BLOCK_COUNT; i++)
                    if (field
                                [current_figure.pos.x +
                                 current_figure.type[i] % figure_bounds.x +
                                 (current_figure.type[i] / figure_bounds.y +
                                  current_figure.pos.y + 1) *
                                     FIELD_WIDTH] == COLOR_BLOCK ||
                        current_figure.type[i] / figure_bounds.y +
                                current_figure.pos.y + 1 >=
                            FIELD_HEIGHT) {
                        do_collide = 1;
                        break;
                    }

                // logic
                if (!do_collide) {
                    current_figure.pos.y++;
                } else {
                    // lose
                    if (current_figure.pos.y == -1) {
                        render_lose();

                        beep_stop();

                        while (1) switch (getch()) {
                            // quit
                            case '\033': goto end;
                            case '\r': goto restart;
                            default:;
                            }
                    }

                    // add figure to field
                    for (i = 0; i < FIGURE_BLOCK_COUNT; i++)
                        field
                            [current_figure.pos.x +
                             current_figure.type[i] % figure_bounds.x +
                             (current_figure.type[i] / figure_bounds.y +
                              current_figure.pos.y) *
                                 FIELD_WIDTH] = COLOR_BLOCK;

                    // destroy filled rows
                    filled_rows_count = 0;
                    for (i = FIELD_HEIGHT - 1; i > 0; i--) {
                        did_fill_row = 1;
                        for (j = 0; j < FIELD_WIDTH; j++) {
                            if (did_fill_row &&
                                field[j + i * FIELD_WIDTH] == COLOR_EMPTY)
                                did_fill_row = 0;

                            if (filled_rows_count == 0) continue;

                            field[j + (i + filled_rows_count) * FIELD_WIDTH] =
                                field[j + i * FIELD_WIDTH];
                            field[j + i * FIELD_WIDTH] = COLOR_EMPTY;
                        }

                        if (did_fill_row) filled_rows_count++;
                    }

                    // add score for filled rows
                    for (; filled_rows_count > 0; filled_rows_count--)
                        score += filled_rows_count;

                    update_score();

                    figure_generate();
                }
            }

            // rendering
            render_main();

            custom_delay(FRAME_DELAY_MS);
        }
    }

end:
    beep_stop();
    fclose(file);

    system("cls");
    free(screen), free(field);

    return 0;
}
