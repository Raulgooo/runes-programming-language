#include "ast.h"
#include "parser.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    Arena arena;
    arena_init(&arena);

    Lexer lexer;
    lexer_init(&lexer, source, NULL);

    Parser parser;
    parser_init(&parser, &lexer, &arena, filename, source);

    AstNode *program = parser_parse(&parser);

    if (program) {
        printf("AST for %s:\n", filename);
        ast_print(program);
    } else {
        fprintf(stderr, "Failed to parse %s\n", filename);
    }

    arena_destroy(&arena);
    free(source);

    return 0;
}
