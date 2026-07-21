#define _XOPEN_SOURCE 700

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "monomorphize.h"
#include "parser.h"
#include "resolver.h"
#include "symbol_table.h"
#include "typecheck.h"
#include "utils/strtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
  bool lex_only;
  bool parse_only;
  bool dump_ast;
  const char *emit_c_filename;
  const char **filenames;
  int file_count;
} Config;

typedef struct {
  Arena *arena;
  StrTab *strtab;
  char **sources;
  char **paths;
  size_t file_count;
  size_t file_capacity;
  bool had_error;
} SourceLoader;

static void source_loader_oom(SourceLoader *loader) {
  if (!loader->had_error)
    fprintf(stderr, "Out of memory while loading Runes source files\n");
  loader->had_error = true;
}

static char *path_directory(const char *path) {
  const char *slash = strrchr(path, '/');
  size_t length = slash ? (size_t)(slash - path) : 1;
  const char *start = slash ? path : ".";
  if (slash == path)
    length = 1;
  char *result = malloc(length + 1);
  if (!result)
    return NULL;
  memcpy(result, start, length);
  result[length] = '\0';
  return result;
}

static char *path_join(const char *left, const char *right) {
  size_t left_length = strlen(left);
  size_t right_length = strlen(right);
  bool separator = left_length && left[left_length - 1] != '/';
  if (left_length > SIZE_MAX - right_length - (separator ? 2 : 1))
    return NULL;
  char *result = malloc(left_length + right_length + (separator ? 2 : 1));
  if (!result)
    return NULL;
  memcpy(result, left, left_length);
  size_t offset = left_length;
  if (separator)
    result[offset++] = '/';
  memcpy(result + offset, right, right_length + 1);
  return result;
}

static bool regular_file_exists(const char *path) {
  struct stat status;
  return stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static bool source_loader_record(SourceLoader *loader, char *path,
                                 char *source) {
  if (loader->file_count == loader->file_capacity) {
    size_t next = loader->file_capacity ? loader->file_capacity * 2 : 8;
    if (next < loader->file_capacity || next > SIZE_MAX / sizeof(char *))
      return false;
    char **sources = realloc(loader->sources, next * sizeof(char *));
    if (!sources)
      return false;
    loader->sources = sources;
    char **paths = realloc(loader->paths, next * sizeof(char *));
    if (!paths)
      return false;
    loader->paths = paths;
    loader->file_capacity = next;
  }
  loader->paths[loader->file_count] = path;
  loader->sources[loader->file_count] = source;
  loader->file_count++;
  return true;
}

static bool source_already_loaded(const SourceLoader *loader,
                                  const char *path) {
  for (size_t i = 0; i < loader->file_count; i++)
    if (strcmp(loader->paths[i], path) == 0)
      return true;
  return false;
}

static AstNode *load_source_program(SourceLoader *loader, const char *path,
                                    const char *module_base);

static char *read_source_file(const char *path, bool *had_error) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    perror(path);
    *had_error = true;
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    fclose(file);
    *had_error = true;
    return NULL;
  }
  long size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    perror("ftell/fseek");
    fclose(file);
    *had_error = true;
    return NULL;
  }
  char *source = malloc((size_t)size + 1);
  if (!source) {
    fclose(file);
    fprintf(stderr, "Out of memory while reading %s\n", path);
    *had_error = true;
    return NULL;
  }
  size_t bytes_read = fread(source, 1, (size_t)size, file);
  fclose(file);
  if (bytes_read != (size_t)size) {
    fprintf(stderr, "Could not read all of %s\n", path);
    free(source);
    *had_error = true;
    return NULL;
  }
  source[size] = '\0';
  return source;
}

static bool load_external_modules(SourceLoader *loader, AstNode *declaration,
                                  const char *module_base) {
  for (; declaration; declaration = declaration->next) {
    if (declaration->kind != AST_MOD_DECL)
      continue;
    const char *name = declaration->as.mod_decl.name;
    char *flat_name = malloc(strlen(name) + sizeof(".runes"));
    if (!flat_name) {
      source_loader_oom(loader);
      return false;
    }
    sprintf(flat_name, "%s.runes", name);
    char *flat = path_join(module_base, flat_name);
    free(flat_name);
    char *directory = path_join(module_base, name);
    char *nested = directory ? path_join(directory, "mod.runes") : NULL;
    if (!flat || !directory || !nested) {
      source_loader_oom(loader);
      free(flat);
      free(directory);
      free(nested);
      return false;
    }

    if (declaration->as.mod_decl.is_external) {
      bool has_flat = regular_file_exists(flat);
      bool has_nested = regular_file_exists(nested);
      if (has_flat == has_nested) {
        fprintf(stderr,
                has_flat
                    ? "Module '%s' is ambiguous: both %s and %s exist\n"
                    : "Module '%s' not found; tried %s and %s\n",
                name, flat, nested);
        loader->had_error = true;
        free(flat);
        free(directory);
        free(nested);
        return false;
      }
      const char *selected = has_flat ? flat : nested;
      char *canonical = realpath(selected, NULL);
      if (!canonical) {
        perror("realpath");
        loader->had_error = true;
        free(flat);
        free(directory);
        free(nested);
        return false;
      }
      if (source_already_loaded(loader, canonical)) {
        fprintf(stderr, "Module file loaded more than once: %s\n", canonical);
        loader->had_error = true;
        free(canonical);
        free(flat);
        free(directory);
        free(nested);
        return false;
      }
      char *child_base =
          has_flat ? strdup(directory) : path_directory(canonical);
      if (!child_base) {
        source_loader_oom(loader);
        free(canonical);
        free(flat);
        free(directory);
        free(nested);
        return false;
      }
      AstNode *program = load_source_program(loader, canonical, child_base);
      free(child_base);
      if (!program) {
        free(canonical);
        free(flat);
        free(directory);
        free(nested);
        return false;
      }
      declaration->as.mod_decl.declarations =
          program->as.program.declarations;
      declaration->as.mod_decl.is_external = false;
      free(canonical);
    } else if (!load_external_modules(loader,
                                      declaration->as.mod_decl.declarations,
                                      directory)) {
      free(flat);
      free(directory);
      free(nested);
      return false;
    }
    free(flat);
    free(directory);
    free(nested);
  }
  return true;
}

static AstNode *load_source_program(SourceLoader *loader, const char *path,
                                    const char *module_base) {
  char *source = read_source_file(path, &loader->had_error);
  if (!source)
    return NULL;
  char *stored_path = strdup(path);
  if (!stored_path || !source_loader_record(loader, stored_path, source)) {
    source_loader_oom(loader);
    free(stored_path);
    free(source);
    return NULL;
  }

  Lexer lexer;
  lexer_init(&lexer, source, loader->strtab);
  Parser parser;
  parser_init(&parser, &lexer, loader->arena, path, source);
  AstNode *program = parser_parse(&parser);
  if (parser.had_error) {
    fprintf(stderr, "Compilation failed in %s with %d error(s)\n", path,
            parser.error_count);
    loader->had_error = true;
    return NULL;
  }
  if (!load_external_modules(loader, program->as.program.declarations,
                             module_base))
    return NULL;
  return program;
}

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
  if (!config.filenames) {
    fprintf(stderr, "Out of memory while parsing command-line arguments\n");
    exit(1);
  }
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
  if (!arena_init(&arena)) {
    fprintf(stderr, "Out of memory while initializing compiler arena\n");
    free(config.filenames);
    return 1;
  }

  StrTab strtab;
  strtab_init(&strtab, &arena);

  AstNode *program = NULL;
  AstNode **next_decl = NULL;
  SourceLoader loader = {.arena = &arena, .strtab = &strtab};

  if (config.lex_only) {
    for (int i = 0; i < config.file_count; i++) {
      char *canonical = realpath(config.filenames[i], NULL);
      if (!canonical) {
        perror(config.filenames[i]);
        had_error = true;
        goto cleanup;
      }
      char *source = read_source_file(canonical, &had_error);
      if (!source) {
        free(canonical);
        goto cleanup;
      }
      Lexer lexer;
      lexer_init(&lexer, source, &strtab);
      Token token;
      do {
        token = lexer_next_token(&lexer);
        printf("[%s] %s at %d:%d: '%.*s'\n", canonical,
               token_kind_to_string(token.kind), token.line, token.column,
               (int)token.length, token.start);
      } while (token.kind != TOKEN_EOF);
      free(source);
      free(canonical);
    }
    goto cleanup;
  }

  for (int i = 0; i < config.file_count; i++) {
    char *canonical = realpath(config.filenames[i], NULL);
    if (!canonical) {
      perror(config.filenames[i]);
      had_error = true;
      goto cleanup;
    }
    if (source_already_loaded(&loader, canonical)) {
      fprintf(stderr, "Source file loaded more than once: %s\n", canonical);
      free(canonical);
      had_error = true;
      goto cleanup;
    }
    char *base = path_directory(canonical);
    if (!base) {
      fprintf(stderr, "Out of memory while resolving source path %s\n",
              canonical);
      free(canonical);
      had_error = true;
      goto cleanup;
    }
    AstNode *file_program = load_source_program(&loader, canonical, base);
    free(base);
    free(canonical);
    if (!file_program) {
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

  if (config.dump_ast && program) {
    ast_print(program);
  }

  if (config.parse_only)
    goto cleanup;

  if (!monomorphize_program(&arena, program)) {
    had_error = true;
    goto cleanup;
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
    codegen_init(&cg, out, &arena);
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
  if (loader.had_error)
    had_error = true;
  arena_destroy(&arena);
  for (size_t i = 0; i < loader.file_count; i++) {
    free(loader.sources[i]);
    free(loader.paths[i]);
  }
  free(loader.sources);
  free(loader.paths);
  free(config.filenames);

  return had_error ? 1 : 0;
}
