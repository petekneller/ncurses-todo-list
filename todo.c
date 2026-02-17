#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "todo_io.h"

/* ── Constants ─────────────────────────────────────────────────────── */

enum {
    PAIR_NORMAL = 1,
    PAIR_DONE,
    PAIR_CURSOR,
    PAIR_CURSOR_DONE,
    PAIR_TITLE,
    PAIR_STATUS,
    PAIR_TAG_BASE   /* = 7; tag pairs 7..13 */
};

#define TAG_COLOR_COUNT 7
static const short TAG_FG_COLORS[TAG_COLOR_COUNT] = {
    COLOR_CYAN, COLOR_MAGENTA, COLOR_RED, COLOR_YELLOW,
    COLOR_GREEN, COLOR_WHITE, COLOR_BLUE
};

/* ── Globals (windows) ─────────────────────────────────────────────── */

static WINDOW *title_win;
static WINDOW *list_win;
static WINDOW *status_win;

/* ── Tag Helpers ───────────────────────────────────────────────────── */

static int tag_color_pair(const AppState *state, int tag_id)
{
    for (int i = 0; i < state->tag_count; i++) {
        if (state->tags[i].id == tag_id)
            return PAIR_TAG_BASE + (i % TAG_COLOR_COUNT);
    }
    return PAIR_NORMAL;
}

static const Tag *find_tag(const AppState *state, int id)
{
    for (int i = 0; i < state->tag_count; i++) {
        if (state->tags[i].id == id)
            return &state->tags[i];
    }
    return NULL;
}

/* ── Data Manipulation ─────────────────────────────────────────────── */

static void todo_add(AppState *state, const char *text)
{
    if (state->count >= MAX_TODOS)
        return;

    Todo *t = &state->items[state->count];
    strncpy(t->text, text, MAX_TODO_TEXT - 1);
    t->text[MAX_TODO_TEXT - 1] = '\0';
    t->done         = 0;
    t->created_at   = time(NULL);
    t->completed_at = 0;
    t->tag_count    = 0;
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
    if (state->items[index].done)
        state->items[index].completed_at = time(NULL);
    else
        state->items[index].completed_at = 0;
}

static void tag_add(AppState *state, const char *name)
{
    if (state->tag_count >= MAX_TAGS)
        return;

    Tag *tag = &state->tags[state->tag_count];
    tag->id = state->tag_next_id++;
    strncpy(tag->name, name, MAX_TAG_TEXT - 1);
    tag->name[MAX_TAG_TEXT - 1] = '\0';
    state->tag_cursor = state->tag_count;
    state->tag_count++;
}

static void tag_delete(AppState *state, int idx)
{
    if (idx < 0 || idx >= state->tag_count)
        return;

    int del_id = state->tags[idx].id;

    /* Remove tag ID from all todos */
    for (int i = 0; i < state->count; i++) {
        Todo *t = &state->items[i];
        for (int j = 0; j < t->tag_count; j++) {
            if (t->tag_ids[j] == del_id) {
                memmove(&t->tag_ids[j], &t->tag_ids[j + 1],
                        (size_t)(t->tag_count - j - 1) * sizeof(int));
                t->tag_count--;
                j--;
            }
        }
    }

    memmove(&state->tags[idx], &state->tags[idx + 1],
            (size_t)(state->tag_count - idx - 1) * sizeof(Tag));
    state->tag_count--;

    if (state->tag_cursor >= state->tag_count && state->tag_count > 0)
        state->tag_cursor = state->tag_count - 1;
    if (state->tag_count == 0)
        state->tag_cursor = 0;
}

/* ── Sorting ───────────────────────────────────────────────────────── */

enum {
    SORT_NONE = 0,
    SORT_NAME_ASC,
    SORT_CREATED_DESC,
    SORT_CREATED_ASC,
    SORT_DONE_DESC,
    SORT_DONE_ASC,
    SORT_MODE_COUNT
};

static const char *sort_label(int mode)
{
    switch (mode) {
    case SORT_NAME_ASC:    return "[Name A-Z]";
    case SORT_CREATED_DESC: return "[Created: new]";
    case SORT_CREATED_ASC:  return "[Created: old]";
    case SORT_DONE_DESC:    return "[Done: new]";
    case SORT_DONE_ASC:     return "[Done: old]";
    default:                return "";
    }
}

static int cmp_name_asc(const void *a, const void *b)
{
    return strcasecmp(((const Todo *)a)->text, ((const Todo *)b)->text);
}

static int cmp_created_desc(const void *a, const void *b)
{
    time_t ta = ((const Todo *)a)->created_at;
    time_t tb = ((const Todo *)b)->created_at;
    return (tb > ta) - (tb < ta);
}

static int cmp_created_asc(const void *a, const void *b)
{
    return cmp_created_desc(b, a);
}

static int cmp_done_desc(const void *a, const void *b)
{
    time_t ta = ((const Todo *)a)->completed_at;
    time_t tb = ((const Todo *)b)->completed_at;
    /* Items with no completion time sort last */
    if (ta == 0 && tb == 0) return 0;
    if (ta == 0) return 1;
    if (tb == 0) return -1;
    return (tb > ta) - (tb < ta);
}

static int cmp_done_asc(const void *a, const void *b)
{
    return cmp_done_desc(b, a);
}

static void apply_sort(AppState *state)
{
    if (state->count < 2)
        return;

    typedef int (*Cmp)(const void *, const void *);
    static const Cmp cmps[SORT_MODE_COUNT] = {
        NULL, cmp_name_asc, cmp_created_desc,
        cmp_created_asc, cmp_done_desc, cmp_done_asc
    };

    Cmp fn = cmps[state->sort_mode];
    if (fn)
        qsort(state->items, (size_t)state->count, sizeof(Todo), fn);
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

    for (int i = 0; i < TAG_COLOR_COUNT; i++)
        init_pair((short)(PAIR_TAG_BASE + i), TAG_FG_COLORS[i], COLOR_BLACK);
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

    /* Tab indicators: active tab gets bold+reverse highlight */
    if (state->active_tab == 0) {
        wattron(win, A_BOLD | A_REVERSE);
        mvwprintw(win, 1, 2, " Todos ");
        wattroff(win, A_BOLD | A_REVERSE);
        mvwprintw(win, 1, 10, " Tags ");
    } else {
        mvwprintw(win, 1, 2, " Todos ");
        wattron(win, A_BOLD | A_REVERSE);
        mvwprintw(win, 1, 10, " Tags ");
        wattroff(win, A_BOLD | A_REVERSE);
    }

    char stats[64];
    snprintf(stats, sizeof(stats), " %d/%d done ", done_count, state->count);
    int cols = getmaxx(win);
    int slen = (int)strlen(stats);
    mvwprintw(win, 1, cols - slen - 2, "%s", stats);

    /* Sort indicator: centered */
    const char *slabel = sort_label(state->sort_mode);
    if (slabel[0] != '\0') {
        int llen = (int)strlen(slabel);
        int lcol = (cols - llen) / 2;
        if (lcol > 18) {  /* don't overlap tabs */
            wattron(win, A_BOLD);
            mvwprintw(win, 1, lcol, "%s", slabel);
            wattroff(win, A_BOLD);
        }
    }

    wrefresh(win);
}

static void draw_list(WINDOW *win, const AppState *state)
{
    werase(win);
    box(win, 0, 0);

    int rows  = getmaxy(win) - 2;  /* usable rows inside border */
    int cols  = getmaxx(win) - 4;  /* used for line-clear width */
    int win_w = getmaxx(win);

    /* Timestamp layout: "MM/DD HH:MM" = 11 chars, flush against right border */
    const int TS_WIDTH = 11;
    int ts_col   = win_w - 1 - TS_WIDTH;   /* start col of timestamp */
    int max_text = ts_col - 1 - 8;         /* 1 gap, text starts at col 8 */

    if (state->count == 0) {
        wattron(win, A_DIM);
        mvwprintw(win, rows / 2, 4, "No todos yet. Press 'a' to add one.");
        wattroff(win, A_DIM);
        wrefresh(win);
        return;
    }

    int vis_row = 0;
    for (int idx = state->scroll_off; idx < state->count && vis_row < rows; idx++) {
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

        /* Format timestamp: completed_at when done, created_at otherwise */
        char ts_str[16] = "";
        time_t ts = t->done ? t->completed_at : t->created_at;
        if (ts > 0) {
            struct tm *tm_info = localtime(&ts);
            if (tm_info)
                strftime(ts_str, sizeof(ts_str), "%m/%d %H:%M", tm_info);
        }

        wattron(win, COLOR_PAIR(pair));
        if (is_cursor)
            wattron(win, A_BOLD);

        /* Clear the todo line */
        wmove(win, vis_row + 1, 1);
        for (int c = 0; c < cols + 2; c++)
            waddch(win, ' ');

        /* Draw cursor indicator, checkbox, and text */
        const char *indicator = is_cursor ? " > " : "   ";
        const char *checkbox  = t->done   ? "[x] " : "[ ] ";

        mvwprintw(win, vis_row + 1, 1, "%s%s", indicator, checkbox);

        /* Truncate text to leave room for timestamp */
        if (max_text >= 4) {
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

        /* Draw timestamp right-aligned */
        if (ts_str[0] != '\0' && ts_col > 9)
            mvwprintw(win, vis_row + 1, ts_col, "%s", ts_str);

        if (is_cursor)
            wattroff(win, A_BOLD);
        wattroff(win, COLOR_PAIR(pair));

        vis_row++;

        /* Draw tag labels row if there are tags and space remains */
        if (t->tag_count > 0 && vis_row < rows) {
            /* Clear the tag line */
            wmove(win, vis_row + 1, 1);
            for (int c = 0; c < cols + 2; c++)
                waddch(win, ' ');

            /* Render [tagname] labels starting at col 9, space-separated */
            int col = 9;
            for (int j = 0; j < t->tag_count; j++) {
                const Tag *tag = find_tag(state, t->tag_ids[j]);
                if (!tag) continue;
                int label_w = (int)strlen(tag->name) + 2; /* "[" + name + "]" */
                if (col + label_w >= win_w - 1) break;
                int cpair = tag_color_pair(state, t->tag_ids[j]);
                wattron(win, COLOR_PAIR(cpair));
                mvwprintw(win, vis_row + 1, col, "[%s]", tag->name);
                wattroff(win, COLOR_PAIR(cpair));
                col += label_w + 1; /* +1 space separator */
            }
            vis_row++;
        }
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

static void draw_tag_manager(WINDOW *win, const AppState *state)
{
    werase(win);
    box(win, 0, 0);

    int rows  = getmaxy(win) - 2;
    int win_w = getmaxx(win);

    if (state->tag_count == 0) {
        wattron(win, A_DIM);
        mvwprintw(win, rows / 2, 4, "No tags yet. Press 'a' to add one.");
        wattroff(win, A_DIM);
        wrefresh(win);
        return;
    }

    for (int i = 0; i < rows && (i + state->tag_scroll_off) < state->tag_count; i++) {
        int idx = i + state->tag_scroll_off;
        const Tag *tag = &state->tags[idx];
        int is_cursor  = (idx == state->tag_cursor);
        int cpair      = PAIR_TAG_BASE + (idx % TAG_COLOR_COUNT);

        /* Clear the row */
        wmove(win, i + 1, 1);
        for (int c = 1; c < win_w - 1; c++)
            waddch(win, ' ');

        const char *indicator = is_cursor ? " > " : "   ";
        mvwprintw(win, i + 1, 1, "%s", indicator);

        if (is_cursor)
            wattron(win, A_BOLD | A_REVERSE);
        else
            wattron(win, COLOR_PAIR(cpair));

        mvwprintw(win, i + 1, 4, "[%s]", tag->name);

        if (is_cursor)
            wattroff(win, A_BOLD | A_REVERSE);
        else
            wattroff(win, COLOR_PAIR(cpair));
    }

    wrefresh(win);
}

static void draw_all(const AppState *state, const char *status_msg)
{
    draw_title(title_win, state);
    if (state->active_tab == 0)
        draw_list(list_win, state);
    else
        draw_tag_manager(list_win, state);
    draw_status(status_win, status_msg);
}

/* ── Scrolling ─────────────────────────────────────────────────────── */

static void adjust_scroll(AppState *state, int visible_rows)
{
    /* Cursor scrolled above view: snap to cursor */
    if (state->cursor < state->scroll_off) {
        state->scroll_off = state->cursor;
        return;
    }

    /* Check whether cursor fits within current scroll offset */
    int rows_used = 0;
    int fits = 0;
    for (int i = state->scroll_off; i < state->count; i++) {
        int h = (state->items[i].tag_count > 0) ? 2 : 1;
        rows_used += h;
        if (i == state->cursor) {
            fits = (rows_used <= visible_rows);
            break;
        }
    }

    if (fits)
        return;

    /* Scroll down: walk backward from cursor filling visible area */
    int accum   = 0;
    int new_off = state->cursor;
    for (int i = state->cursor; i >= 0; i--) {
        int h = (state->items[i].tag_count > 0) ? 2 : 1;
        if (accum + h > visible_rows)
            break;
        accum  += h;
        new_off = i;
    }
    state->scroll_off = new_off;
}

static void adjust_tag_scroll(AppState *state, int visible_rows)
{
    if (state->tag_cursor < state->tag_scroll_off)
        state->tag_scroll_off = state->tag_cursor;
    else if (state->tag_cursor >= state->tag_scroll_off + visible_rows)
        state->tag_scroll_off = state->tag_cursor - visible_rows + 1;
}

/* ── Text Dialog ───────────────────────────────────────────────────── */

/* Generic centered dialog for text input.
   buf is pre-filled with initial text; result is written back into buf.
   Returns 1 on confirm (non-empty), 0 on cancel or empty. */
static int show_text_dialog(const char *title, char *buf, int bufsize)
{
    int dw = COLS * 2 / 3;
    if (dw < 44) dw = 44;
    if (dw > COLS - 4) dw = COLS - 4;
    int dh = 7;
    int dy = (LINES - dh) / 2;
    int dx = (COLS - dw) / 2;

    WINDOW *dlg = newwin(dh, dw, dy, dx);
    keypad(dlg, TRUE);
    wbkgd(dlg, COLOR_PAIR(PAIR_NORMAL));

    int len = (int)strlen(buf);
    int input_cols = dw - 6;

    curs_set(1);

    for (;;) {
        werase(dlg);
        box(dlg, 0, 0);
        mvwprintw(dlg, 0, (dw - (int)strlen(title)) / 2, "%s", title);
        mvwprintw(dlg, 2, 2, "Text:");
        wattron(dlg, A_DIM);
        mvwprintw(dlg, dh - 2, 2, "Enter: confirm   Esc: cancel");
        wattroff(dlg, A_DIM);

        wattron(dlg, COLOR_PAIR(PAIR_CURSOR));
        wmove(dlg, 4, 2);
        for (int c = 0; c < input_cols + 2; c++)
            waddch(dlg, ' ');
        mvwprintw(dlg, 4, 2, ">> %.*s", input_cols - 3, buf);
        wattroff(dlg, COLOR_PAIR(PAIR_CURSOR));

        wrefresh(dlg);

        int ch = wgetch(dlg);

        if (ch == '\n' || ch == KEY_ENTER) {
            curs_set(0);
            delwin(dlg);
            return (len > 0) ? 1 : 0;
        }

        if (ch == 27) {
            curs_set(0);
            delwin(dlg);
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

/* ── Tag Dialog ────────────────────────────────────────────────────── */

static void show_tag_dialog(AppState *state, int todo_idx)
{
    if (todo_idx < 0 || todo_idx >= state->count)
        return;

    Todo *t = &state->items[todo_idx];

    if (state->tag_count == 0) {
        draw_status(status_win, "No tags. Switch to Tags tab (Tab key) to add some.");
        wgetch(list_win);
        return;
    }

    /* Dialog sizing */
    int dw = COLS * 2 / 3;
    if (dw < 40) dw = 40;
    if (dw > COLS - 4) dw = COLS - 4;

    int max_visible = LINES - 8;
    if (max_visible < 3) max_visible = 3;

    int inner = (state->tag_count < max_visible) ? state->tag_count : max_visible;
    int dh    = inner + 4;  /* border rows + hint row */
    int dy    = (LINES - dh) / 2;
    int dx    = (COLS  - dw) / 2;

    WINDOW *dlg = newwin(dh, dw, dy, dx);
    keypad(dlg, TRUE);
    wbkgd(dlg, COLOR_PAIR(PAIR_NORMAL));

    /* Build selection array from current todo tag assignments */
    int selected[MAX_TAGS] = {0};
    for (int i = 0; i < state->tag_count; i++) {
        for (int j = 0; j < t->tag_count; j++) {
            if (state->tags[i].id == t->tag_ids[j]) {
                selected[i] = 1;
                break;
            }
        }
    }

    int dlg_cursor = 0;
    int dlg_scroll = 0;

    for (;;) {
        werase(dlg);
        box(dlg, 0, 0);
        const char *title = " Assign Tags ";
        mvwprintw(dlg, 0, (dw - (int)strlen(title)) / 2, "%s", title);
        wattron(dlg, A_DIM);
        mvwprintw(dlg, dh - 2, 2, "Space:toggle  Enter:confirm  Esc:cancel");
        wattroff(dlg, A_DIM);

        for (int i = 0; i < inner && (i + dlg_scroll) < state->tag_count; i++) {
            int idx      = i + dlg_scroll;
            const Tag *tag = &state->tags[idx];
            int is_cur   = (idx == dlg_cursor);
            int cpair    = PAIR_TAG_BASE + (idx % TAG_COLOR_COUNT);

            /* Clear row */
            wmove(dlg, i + 1, 1);
            for (int c = 1; c < dw - 1; c++)
                waddch(dlg, ' ');

            if (is_cur)
                wattron(dlg, A_BOLD | A_REVERSE);

            const char *check = selected[idx] ? "[x] " : "[ ] ";
            mvwprintw(dlg, i + 1, 2, "%s", check);

            if (!is_cur)
                wattron(dlg, COLOR_PAIR(cpair));
            mvwprintw(dlg, i + 1, 6, "[%s]", tag->name);
            if (!is_cur)
                wattroff(dlg, COLOR_PAIR(cpair));

            if (is_cur)
                wattroff(dlg, A_BOLD | A_REVERSE);
        }

        wrefresh(dlg);

        int ch = wgetch(dlg);

        if (ch == '\n' || ch == KEY_ENTER) {
            /* Write selected IDs back to todo */
            t->tag_count = 0;
            for (int i = 0; i < state->tag_count && t->tag_count < MAX_TODO_TAGS; i++) {
                if (selected[i])
                    t->tag_ids[t->tag_count++] = state->tags[i].id;
            }
            delwin(dlg);
            return;
        }

        if (ch == 27) {
            delwin(dlg);
            return;
        }

        if (ch == ' ')
            selected[dlg_cursor] = !selected[dlg_cursor];

        if ((ch == 'j' || ch == KEY_DOWN) && dlg_cursor < state->tag_count - 1) {
            dlg_cursor++;
            if (dlg_cursor >= dlg_scroll + inner)
                dlg_scroll = dlg_cursor - inner + 1;
        }

        if ((ch == 'k' || ch == KEY_UP) && dlg_cursor > 0) {
            dlg_cursor--;
            if (dlg_cursor < dlg_scroll)
                dlg_scroll = dlg_cursor;
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

    tags_load(&state, TAGS_FILE);
    todos_load(&state, TODO_FILE);
    ui_init();
    ui_create_windows();

    const char *todo_status = "a:Add  e:Edit  t:Tags  d:Delete  Enter:Toggle  j/k:Move  s:Sort  Tab:Tags  q:Quit";
    const char *tag_status  = "a:Add  e:Edit  d:Delete  j/k:Move  Tab:Todos  q:Quit";
    int visible_rows;
    int running = 1;

    draw_all(&state, todo_status);

    while (running) {
        int ch = wgetch(list_win);
        visible_rows = getmaxy(list_win) - 2;

        const char *cur_status = (state.active_tab == 0) ? todo_status : tag_status;

        if (ch == '\t') {
            state.active_tab = !state.active_tab;
            draw_all(&state, state.active_tab == 0 ? todo_status : tag_status);
            continue;
        }

        if (ch == 'q') {
            running = 0;
            break;
        }

        if (ch == KEY_RESIZE) {
            handle_resize(&state);
            draw_all(&state, cur_status);
            continue;
        }

        if (state.active_tab == 0) {
            /* ── Todos tab ── */
            switch (ch) {
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
                char buf[MAX_TODO_TEXT];
                buf[0] = '\0';
                if (show_text_dialog(" Add Todo ", buf, MAX_TODO_TEXT)) {
                    todo_add(&state, buf);
                    adjust_scroll(&state, visible_rows);
                }
                break;
            }

            case 'e':
                if (state.count > 0) {
                    char buf[MAX_TODO_TEXT];
                    strncpy(buf, state.items[state.cursor].text, MAX_TODO_TEXT - 1);
                    buf[MAX_TODO_TEXT - 1] = '\0';
                    if (show_text_dialog(" Edit Todo ", buf, MAX_TODO_TEXT)) {
                        strncpy(state.items[state.cursor].text, buf, MAX_TODO_TEXT - 1);
                        state.items[state.cursor].text[MAX_TODO_TEXT - 1] = '\0';
                    }
                }
                break;

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

            case 't':
                if (state.count > 0)
                    show_tag_dialog(&state, state.cursor);
                break;

            case 's':
                state.sort_mode = (state.sort_mode + 1) % SORT_MODE_COUNT;
                apply_sort(&state);
                state.cursor = 0;
                state.scroll_off = 0;
                break;

            default:
                break;
            }
        } else {
            /* ── Tags tab ── */
            switch (ch) {
            case 'j':
            case KEY_DOWN:
                if (state.tag_count > 0 && state.tag_cursor < state.tag_count - 1) {
                    state.tag_cursor++;
                    adjust_tag_scroll(&state, visible_rows);
                }
                break;

            case 'k':
            case KEY_UP:
                if (state.tag_cursor > 0) {
                    state.tag_cursor--;
                    adjust_tag_scroll(&state, visible_rows);
                }
                break;

            case 'a': {
                char buf[MAX_TAG_TEXT];
                buf[0] = '\0';
                if (show_text_dialog(" Add Tag ", buf, MAX_TAG_TEXT)) {
                    tag_add(&state, buf);
                    adjust_tag_scroll(&state, visible_rows);
                }
                break;
            }

            case 'e':
                if (state.tag_count > 0) {
                    char buf[MAX_TAG_TEXT];
                    strncpy(buf, state.tags[state.tag_cursor].name, MAX_TAG_TEXT - 1);
                    buf[MAX_TAG_TEXT - 1] = '\0';
                    if (show_text_dialog(" Edit Tag ", buf, MAX_TAG_TEXT)) {
                        strncpy(state.tags[state.tag_cursor].name, buf, MAX_TAG_TEXT - 1);
                        state.tags[state.tag_cursor].name[MAX_TAG_TEXT - 1] = '\0';
                    }
                }
                break;

            case 'd':
                if (state.tag_count > 0) {
                    draw_status(status_win, "Delete this tag? (y/n)");
                    int confirm = wgetch(list_win);
                    if (confirm == 'y' || confirm == 'Y') {
                        tag_delete(&state, state.tag_cursor);
                        adjust_tag_scroll(&state, visible_rows);
                    }
                }
                break;

            default:
                break;
            }
        }

        draw_all(&state, cur_status);
    }

    todos_save(&state, TODO_FILE);
    tags_save(&state, TAGS_FILE);
    ui_cleanup();

    return 0;
}
