#include "chibidit.h"

void setStatusMsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(EC.statusmsg, sizeof(EC.statusmsg), fmt, ap);
    va_end(ap);
    EC.statusmsg_time = time(NULL);
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
    for (y = 0; y < EC.screenrows; y++) {
        int filerow = EC.row_offset + y;

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

        int len = r->size - EC.col_offset;
        if (len > 0) {
            if (len > EC.screencols)
                len = EC.screencols;

            char *c = r->render + EC.col_offset;
            for (int j = 0; j < len; j++) {
                abAppend(&ab, c + j, 1);
           }
        }
        abAppend(&ab, "\x1b[39m", 5);
        abAppend(&ab, "\x1b[0K", 4);
        abAppend(&ab, "\r\n", 2);
    }

    // Create a two status rows, First row.
    abAppend(&ab, "\x1b[0K", 4);
    abAppend(&ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
            EC.filename, EC.numrows, EC.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus),
            "%d/%d", EC.row_offset + EC.cy + 1, EC.numrows);

    if (len > EC.screencols)
        len = EC.screencols;
    abAppend(&ab, status, len);
    while (len < EC.screencols) {
        if (EC.screencols - len == rlen) {
            abAppend(&ab, rstatus, rlen);
            break;
        } else {
            abAppend(&ab, " ", 1);
            len++;
        }
    }
    abAppend(&ab, "\x1b[0m\r\n", 6);

    // Second row depends on EC.statusmsg and the status message update time.
    abAppend(&ab, "\x1b[0K", 4);
    int msglen = strlen(EC.statusmsg);
    if (msglen && time(NULL) - EC.statusmsg_time < 5)
        abAppend(&ab, EC.statusmsg, msglen <= EC.screencols ? msglen : EC.screencols);

    // Display cursor at its current position.
    int cx = 1;
    int filerow = EC.row_offset + EC.cy;
    Erow *row = (filerow >= EC.numrows) ? NULL : &EC.row[filerow];
    if (row) {
        for (int j = EC.col_offset; j < (EC.cx + EC.col_offset); j++) {
            if (j < row->size && row->chars[j] == TAB)
                cx += 7 - ((cx) % 8);
            cx++;
        }
    }
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", EC.cy + 1, cx);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
