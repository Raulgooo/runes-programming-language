#include "ast.h"
#include "codegen.h"
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
  const char *emit_c_filename;
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
  fprintf(stderr, "  --emit-c FILE Emit C after successful analysis\n");
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
    } else if (strcmp(argv[i], "--emit-c") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "--emit-c requires an output filename\n");
        print_usage(argv[0]);
        exit(1);
      }
      config.emit_c_filename = argv[++i];
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
  bool had_error = false;

  if (config.file_count == 0) {
    print_usage(argv[0]);
    free(config.filenames);
    return 1;
  }

  Arena arena;
  arena_init(&arena);

  StrTab strtab;
  strtab_init(&strtab, &arena);

  AstNode *program = NULL;
  AstNode **next_decl = NULL;

  char **sources = calloc((size_t)config.file_count, sizeof(char *));
  if (!sources) {
    fprintf(stderr, "Out of memory\n");
    arena_destroy(&arena);
    free(config.filenames);
    return 1;
  }

  for (int i = 0; i < config.file_count; i++) {
    const char *filename = config.filenames[i];
    FILE *f = fopen(filename, "rb");
    if (!f) {
      perror("fopen");
      had_error = true;
      goto cleanup;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
      perror("fseek");
      fclose(f);
      had_error = true;
      goto cleanup;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
      perror("ftell/fseek");
      fclose(f);
      had_error = true;
      goto cleanup;
    }

    char *source = malloc((size_t)size + 1);
    if (!source) {
      fprintf(stderr, "Out of memory\n");
      fclose(f);
      had_error = true;
      goto cleanup;
    }
    size_t bytes_read = fread(source, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) {
      fprintf(stderr, "Could not read all of %s\n", filename);
      free(source);
      fclose(f);
      had_error = true;
      goto cleanup;
    }
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
      had_error = true;
      goto cleanup;
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

  if (config.parse_only)
    goto cleanup;

  // Phase 2: Name Resolution
  SymbolTable st;
  symbol_table_init(&st, &arena);

  Resolver resolver;
  resolver_init(&resolver, &st);
  resolver_resolve(&resolver, program);

  if (resolver.had_error) {
    fprintf(stderr, "Name resolution failed with %d error(s)\n",
            resolver.error_count);
    had_error = true;
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
    had_error = true;
    goto cleanup;
  }

  if (config.emit_c_filename) {
    FILE *out = fopen(config.emit_c_filename, "wb");
    if (!out) {
      perror("fopen");
      had_error = true;
      goto cleanup;
    }

    Codegen cg;
    codegen_init(&cg, out);
    bool ok = codegen_emit_c(&cg, program);
    fclose(out);
    if (!ok) {
      fprintf(stderr, "Code generation failed with %d error(s)\n",
              cg.error_count);
      had_error = true;
      goto cleanup;
    }
  }

cleanup:
  arena_destroy(&arena);
  for (int i = 0; i < config.file_count; i++) {
    free(sources[i]);
  }
  free(sources);
  free(config.filenames);

  return had_error ? 1 : 0;
}
