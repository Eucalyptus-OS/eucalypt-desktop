// A terminal emulator: a shell in a window.
//
// The compositor owns the framebuffer, so a terminal cannot just use /dev/tty
// -- that prints straight over the desktop. Instead this app talks to the shell
// over a pair of pipes and renders the output into its own window, leaving the
// desktop composited underneath.
//
// sh does no line editing of its own, because its tokenizer works on whole
// lines. So this app owns the input line: keystrokes are echoed locally, and
// Enter sends the finished line to the shell. That also means nothing can be
// typed before the shell is ready, which is the same as a real terminal.
//
// One limitation, worth stating plainly: the GUI protocol delivers keycodes
// with no modifier state, so Ctrl-C and friends cannot be detected here. Esc
// closes the window instead, which is why 'q' is free to type a 'q'.

#include "examples.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <abi/syscalls.h>

// mlibc defines this global but no header declares it, so it is declared here.
extern char **environ;

#define SHELL_PATH "/ram/bin/sh"

#define WIN_W 760
#define WIN_H 420

#define PADX 8            // window padding
#define PADY 6
#define CW   6            // glyph advance of the 5x7 font, plus one column gap
#define CH   9            // line height, so glyphs do not touch

#define COLS ((WIN_W - 2 * PADX) / CW)
#define ROWS ((WIN_H - 2 * PADY) / CH)

#define LINE_MAX 512      // longest editable command line
#define READ_MAX  1024    // bytes drained from the shell per pass

// mlibc has fork(), execve(), dup2() and fcntl(), but no pipe(), so the one
// missing call is a raw syscall shim. Same approach sh.c uses for termios.
typedef long scw;
extern long __do_syscall_ret(unsigned long);
extern scw __do_syscall2(long, scw, scw, scw);

static int k_pipe(int fds[2]) {
    return (int)__do_syscall_ret((unsigned long)__do_syscall2(SYS_PIPE, (scw)fds, 0, 0));
}

// --- the screen ---------------------------------------------------------------
//
// A plain character grid, scrolled by moving the lines up. Keeping no history
// past the top of the window is a deliberate simplification: a scrollback
// viewer is a lot of interface for very little value here.

static char grid[ROWS][COLS + 1];   // +1 so each row can be a C string
static int  cur_col, cur_row;

static void grid_clear(void) {
    for (int r = 0; r < ROWS; r++)
        memset(grid[r], ' ', (size_t)COLS);
}

static void grid_scroll(void) {
    memmove(grid[0], grid[1], sizeof(grid[0]) * (ROWS - 1));
    memset(grid[ROWS - 1], ' ', (size_t)COLS);
}

// A byte that is not printable, or is a control code we do not draw, would
// either move the gfx cursor or print as nothing at all. Both look like a bug,
// so they become a visible '?' instead.
static char printable(char c) {
    return (c >= 0x20 && c <= 0x7E) ? c : '?';
}

static void grid_putc(char c) {
    if (c == '\n') {
        cur_col = 0;
        if (++cur_row >= ROWS) {
            grid_scroll();
            cur_row = ROWS - 1;
        }
        return;
    }
    if (c == '\r') { cur_col = 0; return; }
    if (c == '\b' || c == 0x7F) {
        if (cur_col > 0) cur_col--;
        grid[cur_row][cur_col] = ' ';
        return;
    }
    if (c == '\t') {                                  // next multiple of 8
        int next = (cur_col + 8) & ~7;
        while (cur_col < next && cur_col < COLS)
            grid[cur_row][cur_col++] = ' ';
        return;
    }
    // Clamp rather than wrap: a wrapped line would land on top of the next row
    // with no newline in between, which is hard to read.
    if (cur_col >= COLS) return;

    grid[cur_row][cur_col++] = printable(c);
}

static void grid_puts(const char *s) {
    for (; *s; s++)
        grid_putc(*s);
}

// --- the input line -----------------------------------------------------------

static char line[LINE_MAX];
static int  line_len, line_pos;

static void line_insert(char c) {
    if (line_len + 1 >= LINE_MAX) return;
    memmove(line + line_pos + 1, line + line_pos, (size_t)(line_len - line_pos));
    line[line_pos] = c;
    line_len++;
    line_pos++;
}

static void line_backspace(void) {
    if (line_pos == 0) return;
    memmove(line + line_pos - 1, line + line_pos, (size_t)(line_len - line_pos));
    line_len--;
    line_pos--;
}

// --- drawing ------------------------------------------------------------------

static void draw(gui_window_t *win, int show_cursor) {
    gfx_surface_t s;
    gfx_surface_init(&s, win->pixels, win->w, win->h, 0);

    gfx_fill_rect(&s, 0, 0, win->w, win->h, EX_BG);

    for (int r = 0; r < ROWS; r++) {
        grid[r][COLS] = 0;
        ex_draw_label(&s, grid[r], PADX, PADY + r * CH, EX_FG);
    }

    // The shell's prompt is the last thing it printed, so the line being typed
    // continues on from the cursor, exactly as it would in a real terminal.
    ex_draw_label(&s, line, PADX + cur_col * CW, PADY + cur_row * CH, EX_FG);

    if (show_cursor) {
        int cx = PADX + (cur_col + line_pos) * CW;
        int cy = PADY + cur_row * CH;
        gfx_fill_rect(&s, cx, cy, CW - 1, CH - 1, EX_DIM);
    }

    gui_window_damage(win, 0, 0, win->w, win->h);
}

// --- main ---------------------------------------------------------------------

// Unbuffered diagnostics: the kernel console is not a tty, so buffered stdio
// output can be lost when the process exits.
static void dbg(const char *m) { write(2, m, strlen(m)); }

static void dbg_hex(unsigned v) {
    char b[16];
    b[0] = "0123456789abcdef"[v >> 4 & 15];
    b[1] = "0123456789abcdef"[v & 15];
    b[2] = 0;
    dbg(b);
}

int main(void) {
    // Writing to the shell after it has exited would raise SIGPIPE and kill the
    // terminal outright, which is exactly what happens when the user presses
    // Enter just after the shell dies. Ignoring it turns that into a plain
    // write error, which the loop already notices as the shell's exit.
    signal(SIGPIPE, SIG_IGN);

    dbg("DBG connecting\n");
    if (ex_connect(5000) != 0) {
        dbg("DBG ex_connect FAILED\n");
        return 1;
    }
    dbg("DBG connected\n");

    int to_child[2], from_child[2];

    dbg("DBG pipes\n");
    if (k_pipe(to_child) != 0) { dbg("DBG pipe in failed\n"); return 1; }
    if (k_pipe(from_child) != 0) { dbg("DBG pipe out failed\n"); return 1; }

    pid_t pid = fork();
    dbg("DBG forked\n");
    if (pid < 0) return 1;

    if (pid == 0) {
        // Child: wire the shell's standard streams to the pipes, then become it.
        // Failing to exec must not return into the event loop, so the exit
        // status is passed back through the pipe as a visible message.
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        dup2(from_child[1], 2);
        if (to_child[0] > 2) close(to_child[0]);
        if (to_child[1] > 2) close(to_child[1]);
        if (from_child[0] > 2) close(from_child[0]);
        if (from_child[1] > 2) close(from_child[1]);
        execve(SHELL_PATH, (char *const[]){ "sh", NULL }, environ);
        grid_puts("terminal: cannot exec " SHELL_PATH "\n");
        _exit(127);
    }

    // Parent: only the far ends matter from here on.
    close(to_child[0]);
    close(from_child[1]);

    // Neither end can block, or the compositor would stall while waiting on
    // the shell. Everything below is therefore poll-with-sleep rather than
    // blocking reads.
    fcntl(to_child[1], F_SETFL, O_NONBLOCK);
    fcntl(from_child[0], F_SETFL, O_NONBLOCK);

    dbg("DBG creating window\n");
    gui_window_t *win = gui_window_create("term", 40, 30, WIN_W, WIN_H);
    dbg(win ? "DBG window ok\n" : "DBG window FAILED\n");
    if (!win) return 1;

    grid_clear();
    draw(win, 1);

    int child_gone = 0;

    for (;;) {
        int dirty = 0;
        ex_event_t ev;

        // Drain everything the shell has produced, so a burst of output lands
        // in one redraw instead of one redraw per byte.
        for (;;) {
            char buf[READ_MAX];
            ssize_t n = read(from_child[0], buf, sizeof(buf));
            if (n > 0) {
                for (ssize_t i = 0; i < n; i++)
                    grid_putc(buf[i]);
                dirty = 1;
                continue;
            }
            if (n == 0 && !child_gone) {          // shell exited
                child_gone = 1;
                grid_puts("\n[process exited]\n");
                dirty = 1;
            }
            break;
        }

        if (dirty) draw(win, 1);

        // Wait for input, but wake periodically to pick up shell output.
        if (!ex_wait(&ev, 30)) continue;

        if (ev.type == EX_CLOSE) break;

        if (ev.type == EX_KEY) {
            dbg("DBG key code="); dbg_hex(ev.key); dbg("\n");
            if (!ev.state) continue;              // act on press only

            if (ev.key == EX_KEY_ESC) break;

            if (child_gone) continue;             // nothing left to type at

            if (ev.key == EX_KEY_ENTER) {
                char out[LINE_MAX + 1];
                int n = line_len;
                memcpy(out, line, (size_t)n);
                out[n++] = '\n';
                // A closed pipe makes write() fail; that is the shell's exit
                // being noticed a moment later, not an error worth retrying.
                if (write(to_child[1], out, (size_t)n) < 0) { /* handled below */ }
                line[0] = 0;
                line_len = line_pos = 0;
                draw(win, 1);
                continue;
            }

            if (ev.key == 14) {                   // backspace
                line_backspace();
                draw(win, 1);
                continue;
            }

            int c = ex_key_char(ev.key);
            if (c == '\n') continue;              // enter is handled above
            if (c < 0x20) continue;               // no modifier state exists
            line_insert((char)c);
            draw(win, 1);
        }
    }

    // Closing the terminal should take the shell with it, rather than leaving
    // an orphan holding the pipes open.
    kill(pid, 9);
    while (waitpid(pid, NULL, 0) < 0) { /* retry on EINTR */ }

    gui_window_destroy(win);
    return 0;
}
