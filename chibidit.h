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
    Erow *row;
};

static struct EditorConf EC;

enum KEY_ACTIONS {
    TAB,
    ESC,
};

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0};

