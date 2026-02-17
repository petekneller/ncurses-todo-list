#include "todo_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST_FILE "/tmp/todo_test.txt"

static int passed = 0;
static int failed = 0;

static void check(int cond, const char *desc)
{
    if (cond) {
        printf("  PASS: %s\n", desc);
        passed++;
    } else {
        printf("  FAIL: %s\n", desc);
        failed++;
    }
}

static void cleanup(void) { remove(TEST_FILE); }

static void write_file(const char *contents)
{
    FILE *fp = fopen(TEST_FILE, "w");
    fputs(contents, fp);
    fclose(fp);
}

/* ── Tests ─────────────────────────────────────────────────────────── */

static void test_save_empty(void)
{
    printf("save_empty:\n");
    AppState s = {0};
    todos_save(&s, TEST_FILE);

    FILE *fp = fopen(TEST_FILE, "r");
    check(fp != NULL, "file created");
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    check(n == 0, "file is empty");
    cleanup();
}

static void test_save_items(void)
{
    printf("save_items:\n");
    AppState s = {0};
    strcpy(s.items[0].text, "Buy milk");
    s.items[0].done = 0;
    strcpy(s.items[1].text, "Read book");
    s.items[1].done = 1;
    s.count = 2;

    todos_save(&s, TEST_FILE);

    FILE *fp = fopen(TEST_FILE, "r");
    check(fp != NULL, "file created");
    char line1[64], line2[64];
    fgets(line1, sizeof(line1), fp);
    fgets(line2, sizeof(line2), fp);
    fclose(fp);

    check(strcmp(line1, "0 Buy milk\n") == 0,  "undone item formatted correctly");
    check(strcmp(line2, "1 Read book\n") == 0, "done item formatted correctly");
    cleanup();
}

static void test_load_nonexistent(void)
{
    printf("load_nonexistent:\n");
    remove(TEST_FILE);
    AppState s = {0};
    todos_load(&s, TEST_FILE);  /* must not crash */
    check(s.count == 0, "count stays 0 when file missing");
}

static void test_load_items(void)
{
    printf("load_items:\n");
    write_file("0 Buy milk\n1 Read book\n");
    AppState s = {0};
    todos_load(&s, TEST_FILE);

    check(s.count == 2,                              "loaded 2 items");
    check(strcmp(s.items[0].text, "Buy milk") == 0,  "item 0 text");
    check(s.items[0].done == 0,                      "item 0 not done");
    check(strcmp(s.items[1].text, "Read book") == 0, "item 1 text");
    check(s.items[1].done == 1,                      "item 1 done");
    cleanup();
}

static void test_roundtrip(void)
{
    printf("roundtrip:\n");
    AppState orig = {0};
    strcpy(orig.items[0].text, "Task one");
    orig.items[0].done = 0;
    strcpy(orig.items[1].text, "Task two");
    orig.items[1].done = 1;
    orig.count = 2;

    todos_save(&orig, TEST_FILE);

    AppState loaded = {0};
    todos_load(&loaded, TEST_FILE);

    check(loaded.count == orig.count,                          "count matches");
    check(strcmp(loaded.items[0].text, orig.items[0].text) == 0, "item 0 text matches");
    check(loaded.items[0].done == orig.items[0].done,          "item 0 done matches");
    check(strcmp(loaded.items[1].text, orig.items[1].text) == 0, "item 1 text matches");
    check(loaded.items[1].done == orig.items[1].done,          "item 1 done matches");
    cleanup();
}

static void test_load_skips_short_lines(void)
{
    printf("load_skips_short_lines:\n");
    write_file("\n0\n0 Valid item\n");
    AppState s = {0};
    todos_load(&s, TEST_FILE);

    check(s.count == 1,                               "only 1 valid line loaded");
    check(strcmp(s.items[0].text, "Valid item") == 0, "valid item text correct");
    cleanup();
}

static void test_load_resets_count(void)
{
    printf("load_resets_count:\n");
    write_file("0 One\n0 Two\n");
    AppState s = {0};
    s.count = 99;
    todos_load(&s, TEST_FILE);
    check(s.count == 2, "count reset to number of loaded items");
    cleanup();
}

/* ── Entry point ───────────────────────────────────────────────────── */

int main(void)
{
    test_save_empty();
    test_save_items();
    test_load_nonexistent();
    test_load_items();
    test_roundtrip();
    test_load_skips_short_lines();
    test_load_resets_count();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
