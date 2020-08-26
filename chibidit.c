#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "chibidit.h"

void updateRow(Erow *row) {
    unsigned int tabs = 0, nonprint = 0;
    int j, idx;

    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB)
            tabs++;

    row->render = malloc(row->size + tabs * 8 + nonprint * 9 + 1);
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while ((idx + 1) % 8 != 0)
                row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';
}

void insertRow(int at, char *s, size_t len) {
    if (at > EC.numrows) return;
    EC.row = realloc(EC.row, sizeof(Erow) * (EC.numrows + 1));

    EC.row[at].size = len;
    EC.row[at].chars = malloc(len + 1);
    memcpy(EC.row[at].chars, s, len + 1);
    EC.row[at].render = NULL;
    EC.row[at].rsize = 0;
    EC.row[at].idx = at;
    updateRow(EC.row + at);
    EC.numrows++;
}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

void refreshScreen(void) {
    int y;
    Erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    // Hide cursor
    abAppend(&ab, "\x1b[?25l", 6);
    // Go home
    abAppend(&ab, "\x1b[H", 3);
    for (y = 0; y < EC.screenrows + 1; y++) {
        int filerow = EC.rowoff + y;

        // Open initialized editor home
        if (filerow >= EC.numrows) {
            if (EC.numrows == 0 && y == EC.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                        "Welcome, Chibidit Editor\x1b[0K\r\n");
                int padding = (EC.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(&ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(&ab, " ", 1);
                abAppend(&ab, welcome, welcomelen);
                y++;

                char submsg[80];
                int submsg_len = snprintf(submsg, sizeof(submsg),
                        "This is a Toy Text Editor written by C-language.\x1b[0K\r\n");
                int sub_padding = (EC.screencols - submsg_len) / 2;
                if (sub_padding) {
                    abAppend(&ab, "~", 1);
                    padding--;
                }
                while (sub_padding--)
                    abAppend(&ab, " ", 1);
                abAppend(&ab, submsg, submsg_len);
            } else {
                abAppend(&ab, "~\x1b[0K\r\n", 7);
            }
            continue;
        }

        r = &EC.row[filerow];

        int len = r->size - EC.coloff;
        int current_color = -1;
        if (len > 0) {
           if (len > EC.screencols) 
               len = EC.screencols;
           char *c = r->render + EC.coloff;
           for (int j = 0; j < len; j++) {
               abAppend(&ab, c + j, 1);
           }
        }
        abAppend(&ab, "\x1b[39m", 5);
        abAppend(&ab, "\x1b[0K", 4);
        abAppend(&ab, "\r\n", 2);
    }

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

int editorOpen(char *filename) {
    FILE *fp;

    fp = fopen(filename, "r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Not found a file");
            exit(1);
        }
        return 1;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        if (linelen && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            line[--linelen] = '\0';
        insertRow(EC.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    return 0;
}

int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    // Report cursor location
    if (write(ofd, "\x1b[6n", 4) != 4)
        return -1;

    // Read the response: ESC [ row; cols R
    while (i < sizeof(buf) - 1) {
        if (read(ifd, buf + 1, 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    // Parse it.
    if (buf[0] != ESC || buf[1] != '[')
        return -1;
    if (sscanf(buf + 2, "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}

int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // ioctl() failed. Try to query the terminal itself.
        int orig_row, orig_col, retval;

        // Get the initial position so we can restore it later.
        retval = getCursorPosition(ifd, ofd, &orig_row, &orig_col);
        if (retval == -1)
            return -1;

        // Go to right/bottom margin and get position
        if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        retval = getCursorPosition(ifd, ofd, rows, cols);
        if (retval == -1)
            return -1;

        // Restore position
        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        if (write(ofd, seq, strlen(seq)) == -1) {
            // Can't recover...
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void updateWindowSize(void) {
    if (getWindowSize(STDIN_FILENO, STDOUT_FILENO,
                &EC.screenrows, &EC.screencols) == -1) {
        perror("Unable to query the screen for size (columns / rows)");
        exit(1);
    }
    EC.screenrows -= 2;
}

void handleSigWinCh(int unused __attribute__((unused))) {
    updateWindowSize();
    if (EC.cy > EC.screenrows)
        EC.cy = EC.screenrows - 1;

    if (EC.cx > EC.screencols)
        EC.cx = EC.screencols - 1;
    refreshScreen();
}

void initEditor(void) {
    EC.cx = 0;
    EC.cy = 0;
    EC.rowoff = 0;
    EC.coloff = 0;
    EC.numrows = 0;
    EC.row = NULL;
    updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);
}

// In order to restore at exit.
static struct termios orig_termios;

void disableRawMode(int fd) {
    if (EC.rawmode) {
        tcsetattr(fd, TCSAFLUSH, &orig_termios);
        EC.rawmode = 0;
    }
}

void atExit(void) {
    disableRawMode(STDIN_FILENO);
}

int enableRawMode(int fd) {
    struct termios raw;

    // Already enabled.
    if (EC.rawmode)
        return 0;
    atexit(atExit);
    if (tcgetattr(fd, &orig_termios) == -1) {
        errno = ENOTTY;
        return -1;
    }

    // Modify the original mode.
    raw = orig_termios;
    // Input modes: no break, no CR to NL, no parity check, no strip char,
    // no start/stop output control.
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // Output modes - disable post processing.
    raw.c_oflag &= ~(OPOST);
    // Control modes - set 8 bit chars.
    raw.c_cflag |= (CS8);
    // Local modes - choing off, canonical off, no extended functions,
    // no signal chars (^Z, ^C).
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // Control chars - set return condition: min number of bytes and timer.
    raw.c_cc[VMIN] = 0; // Return each byte, or zero for timeout.
    raw.c_cc[VTIME] = 1; // 100 ms timeout (unit is tens of second).

    // Put terminal in raw mode after flushing.
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) {
        errno = ENOTTY;
        return -1;
    }
    EC.rawmode = 1;
    return 0;
}

int readKey(int fd) {
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(fd,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}

char *rowsToString(int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;

    for (int i = 0; i < EC.numrows; i++)
        totlen += EC.row[i].size + 1; // +1 is for "\n" at end of every row
    *buflen = totlen;
    totlen++; // Also make space for nulterm

    p = buf = malloc(totlen);
    for (int j = 0; j < EC.numrows; j++) {
        memcpy(p, EC.row[j].chars, EC.row[j].size);
        p += EC.row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

int save(void) {
    int len;
    char *buf = rowsToString(&len);
}

#define QUIT_TIMES 3
void processKeyPress(int fd) {
    static int quit_times = QUIT_TIMES;

    int c = readKey(fd);
    printf("%d\n", c);
    switch (c) {
    case CTRL_C: // Ignore ctrl-c
        break;
    case CTRL_Q: // Quit
        exit(0);
        break;
    case CTRL_S: // Save
        save();
        break;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: chibidit <filename>\n");
        exit(1);
    }

    initEditor();
    editorOpen(argv[1]);
    enableRawMode(STDIN_FILENO);

    while(1) {
        refreshScreen();
        processKeyPress(STDIN_FILENO);
    }
    
    return 0;
}
