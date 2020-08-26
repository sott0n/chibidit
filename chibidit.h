#define _POSIX_C_SOURCE 200809L

typedef struct Erow {
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
} Erow;

struct EditorConf {
    int cx, cy;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    int rawmode;
    Erow *row;
};

static struct EditorConf EC;

enum KEY_ACTIONS {
        KEY_NULL = 0,       /* NULL */
        CTRL_C = 3,         /* Ctrl-c */
        CTRL_D = 4,         /* Ctrl-d */
        CTRL_F = 6,         /* Ctrl-f */
        CTRL_H = 8,         /* Ctrl-h */
        TAB = 9,            /* Tab */
        CTRL_L = 12,        /* Ctrl+l */
        ENTER = 13,         /* Enter */
        CTRL_Q = 17,        /* Ctrl-q */
        CTRL_S = 19,        /* Ctrl-s */
        CTRL_U = 21,        /* Ctrl-u */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN
};

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0};

