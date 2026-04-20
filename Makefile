CC = gcc
CFLAGS = -Isrc -Wall -Wextra -g

CORE_SRCS = src/lexer.c src/parser.c src/ast.c src/utils/arena.c src/utils/strtab.c src/tools/ast_print.c src/symbol_table.c src/resolver.c src/types.c src/typecheck.c
MAIN_SRC = src/main.c
TARGET = runes

all: $(TARGET)

$(TARGET): $(CORE_SRCS) $(MAIN_SRC)
	$(CC) $(CFLAGS) $(CORE_SRCS) $(MAIN_SRC) -o $(TARGET)

test: $(TARGET)
	./$(TARGET) src/tests/samples/01_variables.runes --parse-only

clean:
	rm -f $(TARGET) lexer_test.exe *.o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
