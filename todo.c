#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Constants ─────────────────────────────────────────────────────── */

#define MAX_TODO_TEXT 256
#define MAX_TODOS    512
#define TODO_FILE    "todos.txt"

enum {
    PAIR_NORMAL = 1,
    PAIR_DONE,
    PAIR_CURSOR,
    PAIR_CURSOR_DONE,
    PAIR_TITLE,
    PAIR_STATUS
};

/* ── Data Structures ───────────────────────────────────────────────── */

typedef struct {
    char text[MAX_TODO_TEXT];
    int  done;
} Todo;

typedef struct {
    Todo items[MAX_TODOS];
    int  count;
    int  cursor;
    int  scroll_off;
} AppState;

/* ── Globals (windows) ─────────────────────────────────────────────── */

static WINDOW *title_win;
static WINDOW *list_win;
static WINDOW *status_win;

/* ── File I/O ──────────────────────────────────────────────────────── */

static void todos_load(AppState *state)
{
    FILE *fp = fopen(TODO_FILE, "r");
    if (!fp)
        return;

    state->count = 0;
    char line[MAX_TODO_TEXT + 16];
    while (fgets(line, sizeof(line), fp) && state->count < MAX_TODOS) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';

        if (len < 2)
            continue;

        int done = (line[0] == '1') ? 1 : 0;
        const char *text = line + 2;  /* skip "0 " or "1 " */

        Todo *t = &state->items[state->count];
        strncpy(t->text, text, MAX_TODO_TEXT - 1);
        t->text[MAX_TODO_TEXT - 1] = '\0';
        t->done = done;
        state->count++;
    }

    fclose(fp);
}

static void todos_save(const AppState *state)
{
    FILE *fp = fopen(TODO_FILE, "w");
    if (!fp)
        return;

    for (int i = 0; i < state->count; i++)
        fprintf(fp, "%d %s\n", state->items[i].done, state->items[i].text);

    fclose(fp);
}

/* ── Data Manipulation ─────────────────────────────────────────────── */

static void todo_add(AppState *state, const char *text)
{
    if (state->count >= MAX_TODOS)
        return;

    Todo *t = &state->items[state->count];
    strncpy(t->text, text, MAX_TODO_TEXT - 1);
    t->text[MAX_TODO_TEXT - 1] = '\0';
    t->done = 0;
    state->cursor = state->count;
    state->count++;
}

static void todo_delete(AppState *state, int index)
{
    if (index < 0 || index >= state->count)
        return;

    memmove(&state->items[index], &state->items[index + 1],
            (size_t)(state->count - index - 1) * sizeof(Todo));
    state->count--;

    if (state->cursor >= state->count && state->count > 0)
        state->cursor = state->count - 1;
    if (state->count == 0)
        state->cursor = 0;
}

static void todo_toggle(AppState *state, int index)
{
    if (index < 0 || index >= state->count)
        return;
    state->items[index].done = !state->items[index].done;
}

/* ── UI Init / Teardown ────────────────────────────────────────────── */

static void ui_init(void)
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(PAIR_NORMAL,      COLOR_WHITE,  COLOR_BLACK);
    init_pair(PAIR_DONE,        COLOR_GREEN,  COLOR_BLACK);
    init_pair(PAIR_CURSOR,      COLOR_BLACK,  COLOR_CYAN);
    init_pair(PAIR_CURSOR_DONE, COLOR_BLACK,  COLOR_GREEN);
    init_pair(PAIR_TITLE,       COLOR_YELLOW, COLOR_BLUE);
    init_pair(PAIR_STATUS,      COLOR_YELLOW, COLOR_BLUE);
}

static void ui_create_windows(void)
{
    int rows = LINES;
    int cols = COLS;

    title_win  = newwin(3, cols, 0, 0);
    list_win   = newwin(rows - 6, cols, 3, 0);
    status_win = newwin(3, cols, rows - 3, 0);

    keypad(list_win, TRUE);

    wbkgd(title_win,  COLOR_PAIR(PAIR_TITLE));
    wbkgd(status_win, COLOR_PAIR(PAIR_STATUS));
}

static void ui_destroy_windows(void)
{
    if (title_win)  { delwin(title_win);  title_win  = NULL; }
    if (list_win)   { delwin(list_win);   list_win   = NULL; }
    if (status_win) { delwin(status_win); status_win = NULL; }
}

static void ui_cleanup(void)
{
    ui_destroy_windows();
    endwin();
}

/* ── Drawing ───────────────────────────────────────────────────────── */

static void draw_title(WINDOW *win, const AppState *state)
{
    werase(win);
    box(win, 0, 0);

    int done_count = 0;
    for (int i = 0; i < state->count; i++)
        if (state->items[i].done)
            done_count++;

    mvwprintw(win, 1, 2, " TODO LIST ");

    char stats[64];
    snprintf(stats, sizeof(stats), " %d/%d done ", done_count, state->count);
    int cols = getmaxx(win);
    int slen = (int)strlen(stats);
    mvwprintw(win, 1, cols - slen - 2, "%s", stats);

    wrefresh(win);
}

static void draw_list(WINDOW *win, const AppState *state)
{
    werase(win);
    box(win, 0, 0);

    int rows = getmaxy(win) - 2;  /* usable rows inside border */
    int cols = getmaxx(win) - 4;  /* usable cols inside border */

    if (state->count == 0) {
        wattron(win, A_DIM);
        mvwprintw(win, rows / 2, 4, "No todos yet. Press 'a' to add one.");
        wattroff(win, A_DIM);
        wrefresh(win);
        return;
    }

    for (int i = 0; i < rows && (i + state->scroll_off) < state->count; i++) {
        int idx = i + state->scroll_off;
        const Todo *t = &state->items[idx];
        int is_cursor = (idx == state->cursor);

        int pair;
        if (is_cursor && t->done)
            pair = PAIR_CURSOR_DONE;
        else if (is_cursor)
            pair = PAIR_CURSOR;
        else if (t->done)
            pair = PAIR_DONE;
        else
            pair = PAIR_NORMAL;

        wattron(win, COLOR_PAIR(pair));
        if (is_cursor)
            wattron(win, A_BOLD);

        /* Clear the line area */
        wmove(win, i + 1, 1);
        for (int c = 0; c < cols + 2; c++)
            waddch(win, ' ');

        /* Draw cursor indicator, checkbox, and text */
        const char *indicator = is_cursor ? " > " : "   ";
        const char *checkbox  = t->done   ? "[x] " : "[ ] ";

        mvwprintw(win, i + 1, 1, "%s%s", indicator, checkbox);

        /* Truncate text if needed */
        int max_text = cols - 7;
        if (max_text > 0) {
            if ((int)strlen(t->text) > max_text) {
                char buf[MAX_TODO_TEXT];
                strncpy(buf, t->text, (size_t)(max_text - 3));
                buf[max_text - 3] = '\0';
                strcat(buf, "...");
                wprintw(win, "%s", buf);
            } else {
                wprintw(win, "%s", t->text);
            }
        }

        if (is_cursor)
            wattroff(win, A_BOLD);
        wattroff(win, COLOR_PAIR(pair));
    }

    wrefresh(win);
}

static void draw_status(WINDOW *win, const char *msg)
{
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, " %s ", msg);
    wrefresh(win);
}

static void draw_all(const AppState *state, const char *status_msg)
{
    draw_title(title_win, state);
    draw_list(list_win, state);
    draw_status(status_win, status_msg);
}

/* ── Scrolling ─────────────────────────────────────────────────────── */

static void adjust_scroll(AppState *state, int visible_rows)
{
    if (state->cursor < state->scroll_off)
        state->scroll_off = state->cursor;
    else if (state->cursor >= state->scroll_off + visible_rows)
        state->scroll_off = state->cursor - visible_rows + 1;
}

/* ── Inline Text Input ─────────────────────────────────────────────── */

static int input_todo_text(WINDOW *win, char *buf, int bufsize)
{
    int len = 0;
    buf[0] = '\0';

    curs_set(1);

    int rows = getmaxy(win) - 2;
    int cols = getmaxx(win) - 4;

    for (;;) {
        /* Draw input line */
        wmove(win, rows, 1);
        wattron(win, COLOR_PAIR(PAIR_CURSOR));
        for (int c = 0; c < cols + 2; c++)
            waddch(win, ' ');
        mvwprintw(win, rows, 2, ">> %s", buf);
        wattroff(win, COLOR_PAIR(PAIR_CURSOR));
        wrefresh(win);

        int ch = wgetch(win);

        if (ch == '\n' || ch == KEY_ENTER) {
            curs_set(0);
            return (len > 0) ? 1 : 0;
        }

        if (ch == 27) {  /* Escape */
            curs_set(0);
            return 0;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (len > 0)
                buf[--len] = '\0';
            continue;
        }

        if (isprint(ch) && len < bufsize - 1) {
            buf[len++] = (char)ch;
            buf[len] = '\0';
        }
    }
}

/* ── Resize Handling ───────────────────────────────────────────────── */

static void handle_resize(AppState *state)
{
    ui_destroy_windows();
    endwin();
    refresh();
    ui_create_windows();

    int visible = getmaxy(list_win) - 2;
    adjust_scroll(state, visible);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    AppState state = {0};

    todos_load(&state);
    ui_init();
    ui_create_windows();

    const char *default_status = "a:Add  d:Delete  Enter:Toggle  j/k:Move  q:Quit";
    int visible_rows;
    int running = 1;

    draw_all(&state, default_status);

    while (running) {
        int ch = wgetch(list_win);
        visible_rows = getmaxy(list_win) - 2;

        switch (ch) {
        case 'q':
            running = 0;
            break;

        case 'j':
        case KEY_DOWN:
            if (state.count > 0 && state.cursor < state.count - 1) {
                state.cursor++;
                adjust_scroll(&state, visible_rows);
            }
            break;

        case 'k':
        case KEY_UP:
            if (state.cursor > 0) {
                state.cursor--;
                adjust_scroll(&state, visible_rows);
            }
            break;

        case '\n':
        case KEY_ENTER:
        case ' ':
            if (state.count > 0)
                todo_toggle(&state, state.cursor);
            break;

        case 'a': {
            draw_status(status_win, "Type todo text, Enter to confirm, Esc to cancel");
            char buf[MAX_TODO_TEXT];
            if (input_todo_text(list_win, buf, MAX_TODO_TEXT)) {
                todo_add(&state, buf);
                adjust_scroll(&state, visible_rows);
            }
            break;
        }

        case 'd':
            if (state.count > 0) {
                draw_status(status_win, "Delete this item? (y/n)");
                int confirm = wgetch(list_win);
                if (confirm == 'y' || confirm == 'Y') {
                    todo_delete(&state, state.cursor);
                    adjust_scroll(&state, visible_rows);
                }
            }
            break;

        case KEY_RESIZE:
            handle_resize(&state);
            break;

        default:
            break;
        }

        draw_all(&state, default_status);
    }

    todos_save(&state);
    ui_cleanup();

    return 0;
}
