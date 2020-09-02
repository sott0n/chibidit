#include "chibidit.h"

struct EditorConf EC;

void initEditor(void) {
    EC.cx = 0;
    EC.cy = 0;
    EC.row_offset = 0;
    EC.col_offset = 0;
    EC.numrows = 0;
    EC.row = NULL;
    EC.dirty = 0;
    EC.filename = NULL;
    updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);
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
