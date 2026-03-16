CC = tcc
CFLAGS = -Isrc

all: lexer_test.exe

lexer_test.exe: src/lexer.c src/tests/lexer_test.c
	$(CC) $(CFLAGS) src/lexer.c src/tests/lexer_test.c -o lexer_test.exe

test: lexer_test.exe
	./lexer_test.exe

clean:
	del /f /q lexer_test.exe test_output.txt
