#ifndef TODO_IO_H
#define TODO_IO_H

#include <time.h>

#define MAX_TODO_TEXT  256
#define MAX_TODOS      512
#define MAX_TAG_TEXT   32
#define MAX_TAGS       64
#define MAX_TODO_TAGS  16
#define TODO_FILE      "todos.txt"
#define TAGS_FILE      "tags.txt"

typedef struct {
    int  id;
    char name[MAX_TAG_TEXT];
} Tag;

typedef struct {
    char   text[MAX_TODO_TEXT];
    int    done;
    time_t created_at;
    time_t completed_at;
    int    tag_ids[MAX_TODO_TAGS];
    int    tag_count;
} Todo;

typedef struct {
    Todo items[MAX_TODOS];
    int  count;
    int  cursor;
    int  scroll_off;
    Tag  tags[MAX_TAGS];
    int  tag_count;
    int  tag_next_id;
    int  tag_cursor;
    int  tag_scroll_off;
    int  active_tab;    /* 0 = todos, 1 = tag manager */
    int  sort_mode;     /* 0=none,1=name,2=created-desc,3=created-asc,4=done-desc,5=done-asc */
} AppState;

void todos_load(AppState *state, const char *path);
void todos_save(const AppState *state, const char *path);
void tags_load(AppState *state, const char *path);
void tags_save(const AppState *state, const char *path);

#endif /* TODO_IO_H */
