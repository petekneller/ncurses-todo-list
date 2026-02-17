#ifndef TODO_IO_H
#define TODO_IO_H

#define MAX_TODO_TEXT 256
#define MAX_TODOS     512
#define TODO_FILE     "todos.txt"

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

void todos_load(AppState *state, const char *path);
void todos_save(const AppState *state, const char *path);

#endif /* TODO_IO_H */
