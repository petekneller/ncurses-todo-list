CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
LDFLAGS = -lncursesw -ltinfo
TARGET  = todo

all: $(TARGET)

$(TARGET): todo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
