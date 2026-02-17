# Project: ncurses Todo List App

A minimal ncurses TUI todo list application in C, built as a learning project for ncurses fundamentals.

## Build & Run

```
make        # compile (gcc, links -lncursesw -ltinfo)
make run    # compile and run
make clean  # remove binary
```

Binary: `./todo`
Data file: `todos.txt` (auto-created on first quit, format: `0 text` or `1 text`)

## Project Structure

| File | Purpose |
|---|---|
| `todo.c` | Single source file (~310 lines) with all logic |
| `Makefile` | Build with gcc, C11, strict warnings |
| `todos.txt` | Persisted todo items (gitignored) |

## Architecture

- **Fixed-size arrays** — `MAX_TODOS=512`, `MAX_TODO_TEXT=256`, no dynamic allocation
- **3 ncurses windows**: `title_win` (3 rows), `list_win` (LINES-6 rows, scrollable), `status_win` (3 rows)
- **6 color pairs**: PAIR_NORMAL, PAIR_DONE, PAIR_CURSOR, PAIR_CURSOR_DONE, PAIR_TITLE, PAIR_STATUS
- **AppState struct** holds items array, count, cursor position, and scroll offset

## Key Bindings

| Key | Action |
|---|---|
| `j` / Down | Move cursor down |
| `k` / Up | Move cursor up |
| `a` | Add new todo (inline text input) |
| `e` | Edit selected todo (centered dialog, pre-filled) |
| `Enter` / Space | Toggle complete/incomplete |
| `d` | Delete (y/n confirmation) |
| `q` | Save and quit |

## Compiler Flags

`-Wall -Wextra -pedantic -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600`

Currently compiles with zero warnings.
