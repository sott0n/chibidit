#include "chibidit.h"

void updateRow(Erow *row) {
    unsigned int tabs = 0, nonprint = 0;
    int j, idx;

    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB)
            tabs++;

    unsigned long long allocsize =
        (unsigned long long) row->size + tabs * 8 + nonprint * 9 + 1;
    if (allocsize > UINT32_MAX) {
        printf("Some line of the edited file is too long for chibidit\n");
        exit(1);
    }

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

    updateSyntaxHighLight(row);
}

// Delete the character at offset 'at' from the specified row.
void rowDelChar(Erow *row, int at) {
    if (row->size <= at)
        return;
    memmove(row->chars + at, row->chars + at + 1, row->size - at);
    updateRow(row);
    row->size--;
    EC.dirty++;
}

void insertRow(int at, char *s, size_t len) {
    if (at > EC.numrows) return;
    EC.row = realloc(EC.row, sizeof(Erow) * (EC.numrows + 1));
    if (at != EC.numrows) {
        memmove(EC.row + at + 1, EC.row + at, sizeof(EC.row[0]) * (EC.numrows - at));
        for (int j = at + 1; j <= EC.numrows; j++)
            EC.row[j].idx++;
    }

    EC.row[at].size = len;
    EC.row[at].chars = malloc(len + 1);
    memcpy(EC.row[at].chars, s, len + 1);
    EC.row[at].hl = NULL;
    EC.row[at].hl_oc = 0;
    EC.row[at].render = NULL;
    EC.row[at].rsize = 0;
    EC.row[at].idx = at;
    updateRow(EC.row + at);
    EC.numrows++;
    EC.dirty++;
}

// Insert a character at the specified position in a row, moving the remaining
// chars on the right if needed.
void rowInsertChar(Erow *row, int at, int c) {
    if (at > row->size) {
        // Pad the string with spaces if the insert location is outside the
        // current length by more than a single character.
        int padlen = at - row->size;
        // In the next line +2 means: new char and null term.
        row->chars = realloc(row->chars, row->size + padlen + 2);
        memset(row->chars + row->size, ' ', padlen);
        row->chars[row->size + padlen + 1] = '\0';
        row->size += padlen + 1;
    } else {
        // If we are in the middle of the string just make space for 1 new
        // char plus the (already existing) null term.
        row->chars = realloc(row->chars, row->size + 2);
        memmove(row->chars + at + 1, row->chars + at, row->size - at + 1);
        row->size++;
    }
    row->chars[at] = c;
    updateRow(row);
    EC.dirty++;
}

void insertNewLine(void) {
    int filerow = EC.row_offset + EC.cy;
    int filecol = EC.col_offset + EC.cx;

    Erow *row = (filerow >= EC.numrows) ? NULL : &EC.row[filerow];

    if (!row) {
        if (filerow == EC.numrows) {
            insertRow(filerow, "", 0);
            goto fixcursor;
        }
        return;
    }

    // If the cursor is over the current line size, we want to conceptually
    // think it's just over the last character.
    if (filecol >= row->size)
        filecol = row->size;
    if (filecol == 0) {
        insertRow(filerow, "", 0);
    } else {
        // We are in the middle of a line. Split it between two rows.
        insertRow(filerow + 1, row->chars + filecol, row->size - filecol);
        row = &EC.row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        updateRow(row);
    }

fixcursor:
    if (EC.cy == EC.screenrows - 1) {
        EC.row_offset++;
    } else {
        EC.cy++;
    }
    EC.cx = 0;
    EC.col_offset = 0;
}

void delChar(void) {
    int filerow = EC.row_offset + EC.cy;
    int filecol = EC.col_offset + EC.cx;
    Erow *row = (filerow > EC.numrows) ? NULL : &EC.row[filerow];

    if (!row || (filecol == 0 && filerow == 0))
        return;
    if (filecol == 0) {
        // Handle the case of column 0, we need to move the current line
        // on the right of the previous one.
        filecol = EC.row[filerow - 1].size;
        rowAppendString(&EC.row[filerow - 1], row->chars, row->size);
        delRow(filerow);
        row = NULL;
        if (EC.cy == 0)
            EC.row_offset--;
        else
            EC.cy--;
        EC.cx = filecol;
        if (EC.cx >= EC.screencols) {
            int shift = (EC.screencols - EC.cx) + 1;
            EC.cx -= shift;
            EC.col_offset += shift;
        }
    } else {
        rowDelChar(row, filecol - 1);
        if (EC.cx == 0 && EC.col_offset)
            EC.col_offset--;
        else
            EC.cx--;
    }
    if (row)
        updateRow(row);
    EC.dirty++;
}

// Append the string 's' at the end of a row
void rowAppendString(Erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(row->chars + row->size, s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    updateRow(row);
    EC.dirty++;
}

void freeRow(Erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

// Remove the row at the specified posision, shifting the remaining
// on the top.
void delRow(int at) {
    Erow *row;

    if (at >= EC.numrows)
        return;
    row = EC.row + at;
    freeRow(row);
    memmove(EC.row + at, EC.row + at + 1, sizeof(EC.row[0]) * (EC.numrows - at - 1));
    for (int j = at; j < EC.numrows - 1; j++)
        EC.row[j].idx++;

    EC.numrows--;
    EC.dirty++;
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

void insertChar(int c) {
    int filerow = EC.row_offset + EC.cy;
    int filecol = EC.col_offset + EC.cx;
    Erow *row = (filerow >= EC.numrows) ? NULL : &EC.row[filerow];

    // If the row where the cursor is currently located does not exist
    // in our logical representation of the file, add enough empty rows
    // as needed.
    if (!row) {
        while(EC.numrows <= filerow)
            insertRow(EC.numrows, "", 0);
    }
    row = &EC.row[filerow];
    rowInsertChar(row, filecol, c);
    if (EC.cx == EC.screencols - 1)
        EC.col_offset++;
    else
        EC.cx++;
    EC.dirty++;
}

