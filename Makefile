CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
LDFLAGS = -lncursesw -ltinfo
TARGET  = todo
TEST    = test_todo_io

all: $(TARGET)

$(TARGET): todo.c todo_io.c todo_io.h
	$(CC) $(CFLAGS) -o $@ todo.c todo_io.c $(LDFLAGS)

$(TEST): test_todo_io.c todo_io.c todo_io.h
	$(CC) $(CFLAGS) -o $@ test_todo_io.c todo_io.c

test: $(TEST)
	./$(TEST)

clean:
	rm -f $(TARGET) $(TEST)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run test
