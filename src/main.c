#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "symbol_table.h"
#include "typecheck.h"
#include "utils/strtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  bool lex_only;
  bool parse_only;
  bool dump_ast;
  const char *filename;
} Config;

static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s [options] <filename>\n", prog);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --lex-only    Only run the lexer and dump tokens\n");
  fprintf(stderr, "  --parse-only  Only run the parser and check syntax\n");
  fprintf(stderr, "  --dump-ast    Parse and dump the Abstract Syntax Tree\n");
}

static Config parse_args(int argc, char **argv) {
  Config config = {0};
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--lex-only") == 0) {
      config.lex_only = true;
    } else if (strcmp(argv[i], "--parse-only") == 0) {
      config.parse_only = true;
    } else if (strcmp(argv[i], "--dump-ast") == 0) {
      config.dump_ast = true;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      exit(1);
    } else {
      if (config.filename) {
        fprintf(stderr, "Multiple files provided: %s and %s\n", config.filename,
                argv[i]);
        print_usage(argv[0]);
        exit(1);
      }
      config.filename = argv[i];
    }
  }
  return config;
}

int main(int argc, char **argv) {
  Config config = parse_args(argc, argv);

  if (!config.filename) {
    print_usage(argv[0]);
    return 1;
  }

  FILE *f = fopen(config.filename, "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *source = malloc(size + 1);
  if (!source) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }
  fread(source, 1, size, f);
  source[size] = '\0';
  fclose(f);

  Arena arena;
  arena_init(&arena);

  StrTab strtab;
  strtab_init(&strtab, &arena);

  Lexer lexer;
  lexer_init(&lexer, source, &strtab);

  if (config.lex_only) {
    Token token;
    do {
      token = lexer_next_token(&lexer);
      printf("%s at %d:%d: '%.*s'\n", token_kind_to_string(token.kind),
             token.line, token.column, (int)token.length, token.start);
    } while (token.kind != TOKEN_EOF);
    goto cleanup;
  }

  Parser parser;
  parser_init(&parser, &lexer, &arena, config.filename, source);

  AstNode *program = parser_parse(&parser);

  if (parser.had_error) {
    fprintf(stderr, "Compilation failed with %d error(s)\n",
            parser.error_count);
    arena_destroy(&arena);
    free(source);
    return 1;
  }

  if (config.dump_ast && program) {
    ast_print(program);
  }

  // Phase 2: Name Resolution
  SymbolTable st;
  symbol_table_init(&st, &arena);

  Resolver resolver;
  resolver_init(&resolver, &st);
  resolver_resolve(&resolver, program);

  if (resolver.had_error) {
    fprintf(stderr, "Name resolution failed with %d error(s)\n",
            resolver.error_count);
    arena_destroy(&arena);
    free(source);
    return 1;
  }

  // Phase 3: Type Checking
  TypeContext tctx;
  type_context_init(&tctx, &arena);

  TypeChecker tc;
  typechecker_init(&tc, &arena, &tctx, &st);
  typechecker_check(&tc, program);

  if (tc.had_error) {
    fprintf(stderr, "Type checking failed with %d error(s)\n", tc.error_count);
    arena_destroy(&arena);
    free(source);
    return 1;
  }

cleanup:
  arena_destroy(&arena);
  free(source);

  return 0;
}
