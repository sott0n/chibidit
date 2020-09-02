#include "chibidit.h"

// In order to restore at exit.
static struct termios orig_termios;

int editorOpen(char *filename) {
    FILE *fp;

    EC.dirty = 0;
    free(EC.filename);
    size_t fnlen = strlen(filename) + 1;
    EC.filename = malloc(fnlen);
    memcpy(EC.filename, filename, fnlen);

    fp = fopen(filename, "r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
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
    EC.dirty = 0;
    return 0;
}

int save(void) {
    int len;
    char *buf = rowsToString(&len);
    int fd = open(EC.filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
        goto err;

    // Use truncate + a single write(2) call in order to make saving
    // a bit safer, under the limits of what we can do in a small editor.
    if (ftruncate(fd, len) == -1)
        goto err;
    if (write(fd, buf, len) != len)
        goto err;

    close(fd);
    free(buf);
    EC.dirty = 0;
    setStatusMsg("%d bytes written on disk", len);
    return 0;

err:
    free(buf);
    if (fd != -1)
        close(fd);
    return 1;
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

void disableRawMode(int fd) {
    if (EC.rawmode) {
        tcsetattr(fd, TCSAFLUSH, &orig_termios);
        EC.rawmode = 0;
    }
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

int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    // Report cursor location
    if (write(ofd, "\x1b[6n", 4) != 4)
        return -1;

    // Read the response: ESC [ row; cols R
    while (i < sizeof(buf) - 1) {
        if (read(ifd, buf + i, 1) != 1)
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

