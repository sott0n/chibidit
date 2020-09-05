#include "chibidit.h"

void moveCursor(int key) {
    int filerow = EC.row_offset + EC.cy;
    int filecol = EC.col_offset + EC.cx;
    int rowlen;
    Erow *row = (filerow >= EC.numrows) ? NULL : &EC.row[filerow];

    switch (key) {
    case ARROW_LEFT:
        if (EC.cx == 0) {
            if (EC.col_offset) {
                EC.col_offset--;
            } else {
                if (filerow > 0) {
                    EC.cy--;
                    EC.cx = EC.row[filerow - 1].size;
                    if (EC.cx > EC.screencols - 1) {
                        EC.col_offset = EC.cx - EC.screencols + 1;
                        EC.cx = EC.screencols - 1;
                    }
                }
            }
        } else {
            EC.cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (EC.cx == EC.screencols - 1)
                EC.col_offset++;
            else
                EC.cx += 1;
        } else if (row && filecol == row->size) {
            EC.cx = 0;
            EC.col_offset = 0;
            if (EC.cy == EC.screenrows - 1)
                EC.row_offset++;
            else
                EC.cx += 1;
        }
        break;
    case ARROW_UP:
        if (EC.cy == 0) {
            if (EC.row_offset)
                EC.row_offset--;
        } else {
            EC.cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < EC.numrows) {
            if (EC.cy == EC.screenrows - 1)
                EC.row_offset++;
            else
                EC.cy += 1;
        }
        break;
    }
    // Fix cx if the current line has not enough chars.
    filerow = EC.row_offset + EC.cy;
    filecol = EC.col_offset + EC.cx;
    row = (filerow >= EC.numrows) ? NULL : &EC.row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        EC.cx -= filecol - rowlen;
        if (EC.cx < 0) {
            EC.col_offset += EC.cx;
            EC.cx = 0;
        }
    }
}

#define QUIT_TIMES 1
void processKeyPress(int fd) {
    static int quit_times = QUIT_TIMES;

    int c = readKey(fd);
    if (EC.mode == NORMAL) {
        switch (c) {
        case CTRL_C: // Ignore ctrl-c
            break;

        case CTRL_Q: // Quit
            // Quit if this file was already saved.
            if (EC.dirty && quit_times) {
                setStatusMsg("WARNING!! File has unsaved changes. "
                        "Press Ctrl-Q %d to quit.", quit_times);
                quit_times--;
                return;
            }
            exit(0);
            break;
        case CTRL_S: // Save
            save();
            break;
        case CTRL_F: // Find mode
            // TODO: implement find string
            break;
        case BACKSPACE:
        case CTRL_H:
        case PAGE_UP:
        case PAGE_DOWN: {
            int times = EC.screenrows;
            if (c == PAGE_UP && EC.cy != 0) {
                EC.cy = 0;
                while(times--)
                    moveCursor(ARROW_UP);
                break;
            }

            if (c == PAGE_DOWN && EC.cy != EC.screenrows - 1) {
                EC.cy = EC.screenrows - 1;
                while(times--)
                    moveCursor(ARROW_DOWN);
                break;
            }
        }
        case DEL_KEY:
            delChar();
            break;
        case DEL_AT_KEY:
            delAtChar();
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
        case CTRL_L: // Clear screen.
            break;
        case ESC:
            break;
        }
    } else if (EC.mode == INSERT) {
        switch(c) {
        case ENTER:  // Enter
            insertNewLine();
            break;
        case DEL_KEY:
            delChar();
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
        case ESC:
            break;
        default:
            insertChar(c);
            break;
        }
    }

    // Reset it to the original time.
    quit_times = QUIT_TIMES;
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

