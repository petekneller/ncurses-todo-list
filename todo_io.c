#include "todo_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void todos_load(AppState *state, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return;

    state->count = 0;
    char line[MAX_TODO_TEXT + 256];
    while (fgets(line, sizeof(line), fp) && state->count < MAX_TODOS) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';

        if (len < 2)
            continue;

        int done = 0;
        long long created = 0, completed = 0;
        int offset = 0;
        const char *text;

        /* Try new format: "<done> <created_ts> <completed_ts> <text>" */
        if (sscanf(line, "%d %lld %lld %n", &done, &created, &completed, &offset) >= 3
                && offset > 0) {
            text = line + offset;
        } else {
            /* Fall back to old format: "<done> <text>" */
            done = (line[0] == '1') ? 1 : 0;
            text = line + 2;
            created = 0;
            completed = 0;
        }

        Todo *t = &state->items[state->count];
        t->tag_count = 0;

        /* Work on a copy to avoid mutating line before text is used */
        char text_copy[MAX_TODO_TEXT + 256];
        strncpy(text_copy, text, sizeof(text_copy) - 1);
        text_copy[sizeof(text_copy) - 1] = '\0';

        /* Check for embedded tag IDs: "\tTAGS:<id1>,<id2>" */
        char *tag_sep = strstr(text_copy, "\tTAGS:");
        if (tag_sep != NULL) {
            *tag_sep = '\0';            /* terminate text at the tab */
            char *id_str = tag_sep + 6; /* skip "\tTAGS:" */
            char *tok = strtok(id_str, ",");
            while (tok && t->tag_count < MAX_TODO_TAGS) {
                t->tag_ids[t->tag_count++] = atoi(tok);
                tok = strtok(NULL, ",");
            }
        }

        strncpy(t->text, text_copy, MAX_TODO_TEXT - 1);
        t->text[MAX_TODO_TEXT - 1] = '\0';
        t->done         = done;
        t->created_at   = (time_t)created;
        t->completed_at = (time_t)completed;
        state->count++;
    }

    fclose(fp);
}

void todos_save(const AppState *state, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return;

    for (int i = 0; i < state->count; i++) {
        const Todo *t = &state->items[i];
        fprintf(fp, "%d %lld %lld %s",
                t->done,
                (long long)t->created_at,
                (long long)t->completed_at,
                t->text);
        if (t->tag_count > 0) {
            fprintf(fp, "\tTAGS:");
            for (int j = 0; j < t->tag_count; j++) {
                if (j > 0) fprintf(fp, ",");
                fprintf(fp, "%d", t->tag_ids[j]);
            }
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

void tags_load(AppState *state, const char *path)
{
    state->tag_count   = 0;
    state->tag_next_id = 1;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return;

    char line[MAX_TAG_TEXT + 16];
    while (fgets(line, sizeof(line), fp) && state->tag_count < MAX_TAGS) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';

        if (len < 2)
            continue;

        int id = 0;
        int offset = 0;
        if (sscanf(line, "%d %n", &id, &offset) < 1 || offset <= 0)
            continue;

        Tag *tag = &state->tags[state->tag_count];
        tag->id = id;
        strncpy(tag->name, line + offset, MAX_TAG_TEXT - 1);
        tag->name[MAX_TAG_TEXT - 1] = '\0';
        state->tag_count++;

        if (id >= state->tag_next_id)
            state->tag_next_id = id + 1;
    }

    fclose(fp);
}

void tags_save(const AppState *state, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return;

    for (int i = 0; i < state->tag_count; i++)
        fprintf(fp, "%d %s\n", state->tags[i].id, state->tags[i].name);

    fclose(fp);
}
