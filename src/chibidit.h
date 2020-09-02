#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

// Syntax highlight types
#define HL_NORMAL 0
#define HL_NONPRINT 1

typedef struct Erow {
    int idx;            /* Row index in the file, zero-based. */
    int size;           /* Size of the row, excluding the null term. */
    int rsize;          /* Size of the rendered row. */
    char *chars;        /* Row content. */
    char *render;       /* Row content "rendered" for screen (for TABs) */
    unsigned char *hl;  /* Syntax highlight type for each character in render. */
} Erow;

struct EditorConf {
    int cx, cy;         /* Cursor x and y position in characters */
    int row_offset;     /* Offset of row displayed */
    int col_offset;     /* Offset of colunm displayed */
    int screenrows;     /* Number of rows that we can show at display */
    int screencols;     /* Number of columns that we can show at display */
    int numrows;        /* Number of rows */
    int rawmode;        /* Is terminal raw mode enabled ? */
    Erow *row;
    int dirty;          /* File modified but not saved. */
    char *filename;     /* Currently open filename. */
    char statusmsg[80];
    time_t statusmsg_time;
};

extern struct EditorConf EC;

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

int editorOpen(char *filename);
int save(void);
int readKey(int fd);
void atExit(void);
int enableRawMode(int fd);
void disableRawMode(int fd);
int getCursorPosition(int ifd, int ofd, int *rows, int *cols);
int getWindowSize(int ifd, int ofd, int *rows, int *cols);
void moveCursor(int key);
void processKeyPress(int fd);
void updateWindowSize(void);
void handleSigWinCh(int unused __attribute__((unused)));
void setStatusMsg(const char *fmt, ...);
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);
void refreshScreen(void);
void updateRow(Erow *row);
void rowDelChar(Erow *row, int at);
void insertRow(int at, char *s, size_t len);
void rowInsertChar(Erow *row, int at, int c);
void insertNewLine(void);
void delChar(void);
void rowAppendString(Erow *row, char *s, size_t len);
void freeRow(Erow *row);
void delRow(int at);
char *rowsToString(int *buflen);
void insertChar(int c);
