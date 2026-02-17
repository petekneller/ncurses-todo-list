#include "todo_io.h"
#include <stdio.h>
#include <string.h>

void todos_load(AppState *state, const char *path)
{
    FILE *fp = fopen(path, "r");
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

void todos_save(const AppState *state, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return;

    for (int i = 0; i < state->count; i++)
        fprintf(fp, "%d %s\n", state->items[i].done, state->items[i].text);

    fclose(fp);
}
