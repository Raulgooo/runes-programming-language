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
  const char **filenames;
  int file_count;
} Config;

static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s [options] <filename> [additional_files...]\n",
          prog);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --lex-only    Only run the lexer and dump tokens\n");
  fprintf(stderr, "  --parse-only  Only run the parser and check syntax\n");
  fprintf(stderr, "  --dump-ast    Parse and dump the Abstract Syntax Tree\n");
}

static Config parse_args(int argc, char **argv) {
  Config config = {0};
  config.filenames = malloc(sizeof(const char *) * argc);
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
      config.filenames[config.file_count++] = argv[i];
    }
  }
  return config;
}

int main(int argc, char **argv) {
  Config config = parse_args(argc, argv);

  if (config.file_count == 0) {
    print_usage(argv[0]);
    return 1;
  }

  Arena arena;
  arena_init(&arena);

  StrTab strtab;
  strtab_init(&strtab, &arena);

  AstNode *program = NULL;
  AstNode **next_decl = NULL;

  char **sources = malloc(sizeof(char *) * config.file_count);

  for (int i = 0; i < config.file_count; i++) {
    const char *filename = config.filenames[i];
    FILE *f = fopen(filename, "rb");
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
    sources[i] = source;

    Lexer lexer;
    lexer_init(&lexer, source, &strtab);

    if (config.lex_only) {
      Token token;
      do {
        token = lexer_next_token(&lexer);
        printf("[%s] %s at %d:%d: '%.*s'\n", filename,
               token_kind_to_string(token.kind), token.line, token.column,
               (int)token.length, token.start);
      } while (token.kind != TOKEN_EOF);
      continue;
    }

    Parser parser;
    parser_init(&parser, &lexer, &arena, filename, source);

    AstNode *file_program = parser_parse(&parser);

    if (parser.had_error) {
      fprintf(stderr, "Compilation failed in %s with %d error(s)\n", filename,
              parser.error_count);
      return 1;
    }

    if (!program) {
      program = file_program;
    } else {
      // Append declarations to existing program
      if (file_program && file_program->kind == AST_PROGRAM) {
        if (next_decl) {
          *next_decl = file_program->as.program.declarations;
        }
      }
    }

    // Update next_decl pointer to the end of the current declarations list
    if (program && program->kind == AST_PROGRAM) {
      next_decl = &program->as.program.declarations;
      while (*next_decl) {
        next_decl = &((*next_decl)->next);
      }
    }
  }

  if (config.lex_only)
    goto cleanup;

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
    goto cleanup;
  }

  // Phase 3: Type Checking
  TypeContext tctx;
  type_context_init(&tctx, &arena);

  TypeChecker tc;
  typechecker_init(&tc, &arena, &tctx, &st);
  typechecker_check(&tc, program);

  if (tc.had_error) {
    fprintf(stderr, "Type checking failed with %d error(s)\n", tc.error_count);
    goto cleanup;
  }

cleanup:
  arena_destroy(&arena);
  for (int i = 0; i < config.file_count; i++) {
    free(sources[i]);
  }
  free(sources);
  free(config.filenames);

  return (resolver.had_error || tc.had_error) ? 1 : 0;
}
