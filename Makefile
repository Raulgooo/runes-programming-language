CC = gcc
CFLAGS = -Isrc -Wall -Wextra

SRCS = src/lexer.c src/utils/arena.c src/utils/strtab.c src/tests/lexer_test.c
TARGET = lexer_test

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) lexer_test.exe *.o
