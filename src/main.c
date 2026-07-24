#define _XOPEN_SOURCE 700

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "monomorphize.h"
#include "parser.h"
#include "project.h"
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
  bool with_prelude;
  bool no_prelude;
  bool project_info;
  const char *emit_c_filename;
  const char *project_filename;
  const char *stdlib_path;
  const char **filenames;
  int file_count;
  const char **module_paths;
  int module_path_count;
} Config;

typedef struct {
  Arena *arena;
  StrTab *strtab;
  char **sources;
  char **paths;
  AstNode **programs;
  bool *loading;
  size_t file_count;
  size_t file_capacity;
  const char **module_paths;
  size_t module_path_count;
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
    AstNode **programs = realloc(loader->programs, next * sizeof(AstNode *));
    if (!programs)
      return false;
    loader->programs = programs;
    bool *loading = realloc(loader->loading, next * sizeof(bool));
    if (!loading)
      return false;
    loader->loading = loading;
    loader->file_capacity = next;
  }
  loader->paths[loader->file_count] = path;
  loader->sources[loader->file_count] = source;
  loader->programs[loader->file_count] = NULL;
  loader->loading[loader->file_count] = true;
  loader->file_count++;
  return true;
}

static int source_loader_index(const SourceLoader *loader, const char *path) {
  for (size_t i = 0; i < loader->file_count; i++)
    if (strcmp(loader->paths[i], path) == 0)
      return (int)i;
  return -1;
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

typedef struct {
  char *source_path;
  char *child_base;
} ModuleLocation;

static int locate_module_in_directory(SourceLoader *loader, const char *name,
                                      const char *base,
                                      ModuleLocation *location) {
  char *flat_name = malloc(strlen(name) + sizeof(".runes"));
  if (!flat_name) {
    source_loader_oom(loader);
    return -1;
  }
  sprintf(flat_name, "%s.runes", name);
  char *flat = path_join(base, flat_name);
  free(flat_name);
  char *directory = path_join(base, name);
  char *nested = directory ? path_join(directory, "mod.runes") : NULL;
  if (!flat || !directory || !nested) {
    source_loader_oom(loader);
    free(flat);
    free(directory);
    free(nested);
    return -1;
  }
  bool has_flat = regular_file_exists(flat);
  bool has_nested = regular_file_exists(nested);
  if (has_flat && has_nested) {
    fprintf(stderr, "Module '%s' is ambiguous: both %s and %s exist\n", name,
            flat, nested);
    loader->had_error = true;
    free(flat);
    free(directory);
    free(nested);
    return -1;
  }
  if (!has_flat && !has_nested) {
    free(flat);
    free(directory);
    free(nested);
    return 0;
  }
  location->source_path = has_flat ? flat : nested;
  location->child_base = has_flat ? directory : path_directory(nested);
  if (has_flat)
    free(nested);
  else {
    free(flat);
    free(directory);
  }
  if (!location->child_base) {
    source_loader_oom(loader);
    free(location->source_path);
    location->source_path = NULL;
    return -1;
  }
  return 1;
}

static bool locate_module(SourceLoader *loader, const char *name,
                          const char *module_base, ModuleLocation *location) {
  memset(location, 0, sizeof(*location));
  int local = locate_module_in_directory(loader, name, module_base, location);
  if (local < 0)
    return false;
  if (local > 0)
    return true;

  for (size_t i = 0; i < loader->module_path_count; i++) {
    ModuleLocation candidate = {0};
    int found = locate_module_in_directory(loader, name,
                                           loader->module_paths[i], &candidate);
    if (found < 0) {
      free(location->source_path);
      free(location->child_base);
      return false;
    }
    if (!found)
      continue;
    if (location->source_path) {
      fprintf(stderr,
              "Module '%s' is ambiguous across module roots: %s and %s\n", name,
              location->source_path, candidate.source_path);
      loader->had_error = true;
      free(candidate.source_path);
      free(candidate.child_base);
      free(location->source_path);
      free(location->child_base);
      location->source_path = NULL;
      location->child_base = NULL;
      return false;
    }
    *location = candidate;
  }
  if (!location->source_path) {
    fprintf(stderr, "Module '%s' not found from %s", name, module_base);
    if (loader->module_path_count) {
      fprintf(stderr, " or configured module roots:");
      for (size_t i = 0; i < loader->module_path_count; i++)
        fprintf(stderr, " %s", loader->module_paths[i]);
    }
    fputc('\n', stderr);
    loader->had_error = true;
    return false;
  }
  return true;
}

static bool load_external_modules(SourceLoader *loader, AstNode *declaration,
                                  const char *module_base) {
  for (; declaration; declaration = declaration->next) {
    if (declaration->kind != AST_MOD_DECL)
      continue;
    const char *name = declaration->as.mod_decl.name;
    char *directory = path_join(module_base, name);
    if (!directory) {
      source_loader_oom(loader);
      return false;
    }

    if (declaration->as.mod_decl.is_external) {
      ModuleLocation location;
      if (!locate_module(loader, name, module_base, &location)) {
        free(directory);
        return false;
      }
      char *canonical = realpath(location.source_path, NULL);
      if (!canonical) {
        perror("realpath");
        loader->had_error = true;
        free(location.source_path);
        free(location.child_base);
        free(directory);
        return false;
      }
      AstNode *program =
          load_source_program(loader, canonical, location.child_base);
      if (!program) {
        free(canonical);
        free(location.source_path);
        free(location.child_base);
        free(directory);
        return false;
      }
      declaration->as.mod_decl.declarations = program->as.program.declarations;
      declaration->as.mod_decl.is_external = false;
      free(canonical);
      free(location.source_path);
      free(location.child_base);
    } else if (!load_external_modules(
                   loader, declaration->as.mod_decl.declarations, directory)) {
      free(directory);
      return false;
    }
    free(directory);
  }
  return true;
}

static AstNode *load_source_program(SourceLoader *loader, const char *path,
                                    const char *module_base) {
  int existing = source_loader_index(loader, path);
  if (existing >= 0) {
    if (loader->loading[existing]) {
      fprintf(stderr, "Cyclic module dependency:");
      for (size_t i = (size_t)existing; i < loader->file_count; i++)
        if (loader->loading[i])
          fprintf(stderr, " %s ->", loader->paths[i]);
      fprintf(stderr, " %s\n", path);
      loader->had_error = true;
      return NULL;
    }
    return loader->programs[existing];
  }
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
  size_t index = loader->file_count - 1;
  loader->programs[index] = program;
  if (!load_external_modules(loader, program->as.program.declarations,
                             module_base))
    return NULL;
  loader->loading[index] = false;
  return program;
}

static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s [options] [filename] [additional_files...]\n",
          prog);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --lex-only    Only run the lexer and dump tokens\n");
  fprintf(stderr, "  --parse-only  Only run the parser and check syntax\n");
  fprintf(stderr, "  --dump-ast    Parse and dump the Abstract Syntax Tree\n");
  fprintf(stderr, "  --emit-c FILE Emit C after successful analysis\n");
  fprintf(stderr, "  --project FILE Use an explicit runes.toml manifest\n");
  fprintf(stderr, "  --module-path DIR Add a module search root\n");
  fprintf(stderr, "  --stdlib DIR Use DIR as the standard library root\n");
  fprintf(stderr, "  --prelude Load the standard runtime declarations\n");
  fprintf(stderr, "  --no-prelude Do not load runtime declarations\n");
  fprintf(stderr, "  --project-info Print resolved project configuration\n");
}

static Config parse_args(int argc, char **argv) {
  Config config = {0};
  config.filenames = malloc(sizeof(const char *) * argc);
  config.module_paths = malloc(sizeof(const char *) * argc);
  if (!config.filenames || !config.module_paths) {
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
    } else if (strcmp(argv[i], "--prelude") == 0) {
      config.with_prelude = true;
    } else if (strcmp(argv[i], "--no-prelude") == 0) {
      config.no_prelude = true;
    } else if (strcmp(argv[i], "--project-info") == 0) {
      config.project_info = true;
    } else if (strcmp(argv[i], "--emit-c") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "--emit-c requires an output filename\n");
        print_usage(argv[0]);
        exit(1);
      }
      config.emit_c_filename = argv[++i];
    } else if (strcmp(argv[i], "--project") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "--project requires a manifest filename\n");
        exit(1);
      }
      config.project_filename = argv[++i];
    } else if (strcmp(argv[i], "--module-path") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "--module-path requires a directory\n");
        exit(1);
      }
      config.module_paths[config.module_path_count++] = argv[++i];
    } else if (strcmp(argv[i], "--stdlib") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "--stdlib requires a directory\n");
        exit(1);
      }
      config.stdlib_path = argv[++i];
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      exit(1);
    } else {
      config.filenames[config.file_count++] = argv[i];
    }
  }
  if (config.with_prelude && config.no_prelude) {
    fprintf(stderr, "--prelude and --no-prelude cannot be used together\n");
    exit(1);
  }
  return config;
}

static bool directory_contains(const char *directory, const char *filename) {
  char *path = path_join(directory, filename);
  bool exists = path && regular_file_exists(path);
  free(path);
  return exists;
}

static char *canonical_stdlib_candidate(const char *candidate) {
  char *canonical = realpath(candidate, NULL);
  if (!canonical)
    return NULL;
  if (!directory_contains(canonical, "mod.runes")) {
    free(canonical);
    return NULL;
  }
  return canonical;
}

static char *discover_stdlib(const Config *config, const char *executable) {
  if (config->stdlib_path) {
    char *result = canonical_stdlib_candidate(config->stdlib_path);
    if (!result)
      fprintf(stderr, "Invalid standard library directory: %s\n",
              config->stdlib_path);
    return result;
  }
  const char *environment = getenv("RUNES_STDLIB");
  if (environment && *environment) {
    char *result = canonical_stdlib_candidate(environment);
    if (!result)
      fprintf(stderr, "Invalid RUNES_STDLIB directory: %s\n", environment);
    return result;
  }
  char *canonical_executable = realpath(executable, NULL);
  if (!canonical_executable)
    return NULL;
  char *binary_directory = path_directory(canonical_executable);
  free(canonical_executable);
  if (!binary_directory)
    return NULL;
  char *development = path_join(binary_directory, "src/std");
  char *installed = path_join(binary_directory, "../lib/runes/std");
  free(binary_directory);
  char *result = development ? canonical_stdlib_candidate(development) : NULL;
  if (!result && installed)
    result = canonical_stdlib_candidate(installed);
  free(development);
  free(installed);
  return result;
}

static bool append_unique_module_path(char **paths, size_t *count,
                                      char *canonical) {
  if (!canonical)
    return false;
  for (size_t i = 0; i < *count; i++) {
    if (strcmp(paths[i], canonical) == 0) {
      free(canonical);
      return true;
    }
  }
  paths[(*count)++] = canonical;
  return true;
}

static bool path_is_within_directory(const char *path, const char *directory) {
  if (!path || !directory)
    return false;
  size_t directory_length = strlen(directory);
  while (directory_length > 1 && directory[directory_length - 1] == '/')
    directory_length--;
  return strncmp(path, directory, directory_length) == 0 &&
         (path[directory_length] == '/' || path[directory_length] == '\0');
}

static bool append_program_declarations(AstNode **program,
                                        AstNode ***next_declaration,
                                        AstNode *addition) {
  if (!addition || addition->kind != AST_PROGRAM)
    return false;
  if (!*program)
    *program = addition;
  else if (*next_declaration)
    **next_declaration = addition->as.program.declarations;
  *next_declaration = &(*program)->as.program.declarations;
  while (**next_declaration)
    *next_declaration = &(**next_declaration)->next;
  return true;
}

static AstNode *load_named_root_module(SourceLoader *loader, Arena *arena,
                                       const char *name, const char *root) {
  char *module_file = NULL;
  char *module_base = NULL;
  if (regular_file_exists(root)) {
    module_file = strdup(root);
    module_base = path_directory(root);
  } else {
    module_file = path_join(root, "mod.runes");
    module_base = strdup(root);
  }
  if (!module_file || !module_base || !regular_file_exists(module_file)) {
    fprintf(stderr, "Module dependency '%s' has no mod.runes at %s\n", name,
            root);
    free(module_file);
    free(module_base);
    loader->had_error = true;
    return NULL;
  }
  char *canonical = realpath(module_file, NULL);
  free(module_file);
  if (!canonical) {
    perror("realpath");
    free(module_base);
    loader->had_error = true;
    return NULL;
  }
  AstNode *loaded = load_source_program(loader, canonical, module_base);
  free(canonical);
  free(module_base);
  if (!loaded)
    return NULL;
  const char *interned_name = strtab_intern(loader->strtab, name, strlen(name));
  AstNode *module = ast_new_mod_decl(arena, true, interned_name,
                                     loaded->as.program.declarations);
  module->as.mod_decl.is_external = false;
  AstNode *wrapper = ast_new_program(arena, module);
  return wrapper;
}

int main(int argc, char **argv) {
  Config config = parse_args(argc, argv);
  bool had_error = false;
  RunesProject project;
  runes_project_init(&project);
  bool has_project = false;
  char *discovered_manifest = NULL;
  char *stdlib_root = NULL;
  char **module_paths = NULL;
  size_t module_path_count = 0;

  if (config.project_filename)
    discovered_manifest = strdup(config.project_filename);
  else
    discovered_manifest = runes_project_discover(".");
  if (discovered_manifest) {
    if (!runes_project_load(&project, discovered_manifest)) {
      had_error = true;
      goto early_cleanup;
    }
    has_project = true;
  }
  if (config.file_count == 0 && !config.project_info) {
    if (!has_project) {
      fprintf(stderr, "No input file and no runes.toml project found\n");
      print_usage(argv[0]);
      had_error = true;
      goto early_cleanup;
    }
    config.filenames[config.file_count++] = project.entry;
  }
  if (has_project && !config.no_prelude)
    config.with_prelude = true;

  stdlib_root = discover_stdlib(&config, argv[0]);
  if ((config.stdlib_path || getenv("RUNES_STDLIB") || config.with_prelude) &&
      !stdlib_root) {
    had_error = true;
    goto early_cleanup;
  }

  size_t maximum_paths =
      project.module_root_count + (size_t)config.module_path_count + 1;
  const char *environment_paths = getenv("RUNES_PATH");
  if (environment_paths)
    for (const char *p = environment_paths; *p; p++)
      if (*p == ':')
        maximum_paths++;
  if (environment_paths && *environment_paths)
    maximum_paths++;
  module_paths = calloc(maximum_paths ? maximum_paths : 1, sizeof(char *));
  if (!module_paths) {
    fprintf(stderr, "Out of memory while configuring module paths\n");
    had_error = true;
    goto early_cleanup;
  }
  for (size_t i = 0; i < project.module_root_count; i++) {
    if (!append_unique_module_path(module_paths, &module_path_count,
                                   strdup(project.module_roots[i]))) {
      fprintf(stderr, "Out of memory while configuring module paths\n");
      had_error = true;
      goto early_cleanup;
    }
  }
  for (int i = 0; i < config.module_path_count; i++) {
    char *canonical = realpath(config.module_paths[i], NULL);
    if (!canonical) {
      perror(config.module_paths[i]);
      had_error = true;
      goto early_cleanup;
    }
    append_unique_module_path(module_paths, &module_path_count, canonical);
  }
  if (environment_paths && *environment_paths) {
    char *copy = strdup(environment_paths);
    char *state = NULL;
    if (!copy) {
      had_error = true;
      goto early_cleanup;
    }
    for (char *part = strtok_r(copy, ":", &state); part;
         part = strtok_r(NULL, ":", &state)) {
      char *canonical = realpath(part, NULL);
      if (!canonical) {
        perror(part);
        free(copy);
        had_error = true;
        goto early_cleanup;
      }
      append_unique_module_path(module_paths, &module_path_count, canonical);
    }
    free(copy);
  }

  if (config.project_info) {
    if (!has_project) {
      fprintf(stderr, "No runes.toml project found\n");
      had_error = true;
      goto early_cleanup;
    }
    printf("manifest: %s\n", project.manifest_path);
    printf("project: %s\n", project.name);
    printf("entry: %s\n", project.entry);
    printf("stdlib: %s\n", stdlib_root ? stdlib_root : "<not found>");
    printf("prelude: %s\n", config.with_prelude ? "enabled" : "disabled");
    for (size_t i = 0; i < module_path_count; i++)
      printf("module-root: %s\n", module_paths[i]);
    for (size_t i = 0; i < project.dependency_count; i++)
      printf("dependency: %s = %s\n", project.dependency_names[i],
             project.dependency_paths[i]);
    goto early_cleanup;
  }

  Arena arena;
  if (!arena_init(&arena)) {
    fprintf(stderr, "Out of memory while initializing compiler arena\n");
    had_error = true;
    goto early_cleanup;
  }

  StrTab strtab;
  strtab_init(&strtab, &arena);

  AstNode *program = NULL;
  AstNode **next_decl = NULL;
  SourceLoader loader = {.arena = &arena,
                         .strtab = &strtab,
                         .module_paths = (const char **)module_paths,
                         .module_path_count = module_path_count};

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

  if (config.with_prelude) {
    char *prelude = path_join(stdlib_root, "prelude.runes");
    char *canonical = prelude ? realpath(prelude, NULL) : NULL;
    free(prelude);
    if (!canonical) {
      fprintf(stderr, "Standard library has no prelude.runes: %s\n",
              stdlib_root);
      had_error = true;
      goto cleanup;
    }
    AstNode *prelude_program =
        load_source_program(&loader, canonical, stdlib_root);
    free(canonical);
    if (!prelude_program ||
        !append_program_declarations(&program, &next_decl, prelude_program)) {
      had_error = true;
      goto cleanup;
    }
  }

  bool inject_std_namespace = stdlib_root != NULL;
  if (inject_std_namespace) {
    char *standard_manifest = path_join(stdlib_root, "mod.runes");
    char *canonical_standard =
        standard_manifest ? realpath(standard_manifest, NULL) : NULL;
    free(standard_manifest);
    for (int i = 0; canonical_standard && i < config.file_count; i++) {
      char *canonical_input = realpath(config.filenames[i], NULL);
      if (canonical_input && strcmp(canonical_input, canonical_standard) == 0)
        inject_std_namespace = false;
      free(canonical_input);
    }
    free(canonical_standard);
  }

  /*
   * Load the standard namespace before explicit inputs. A source file beneath
   * the stdlib root may also be named directly on the command line while
   * developing the library. Loading the input first and then reusing its AST
   * inside `std` would let append_program_declarations splice the same linked
   * declaration list into two parents, creating a cycle.
   */
  if (inject_std_namespace) {
    AstNode *standard =
        load_named_root_module(&loader, &arena, "std", stdlib_root);
    if (!standard ||
        !append_program_declarations(&program, &next_decl, standard)) {
      had_error = true;
      goto cleanup;
    }
  }

  for (int i = 0; i < config.file_count; i++) {
    char *canonical = realpath(config.filenames[i], NULL);
    if (!canonical) {
      perror(config.filenames[i]);
      had_error = true;
      goto cleanup;
    }
    if (source_loader_index(&loader, canonical) >= 0) {
      if (inject_std_namespace &&
          path_is_within_directory(canonical, stdlib_root)) {
        /*
         * The file is already reachable through std/mod.runes. Analyze that
         * single AST instance in its module context; do not append its
         * declarations again at the program root.
         */
        free(canonical);
        continue;
      }
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

    if (!append_program_declarations(&program, &next_decl, file_program)) {
      had_error = true;
      goto cleanup;
    }
  }

  for (size_t i = 0; i < project.dependency_count; i++) {
    AstNode *dependency =
        load_named_root_module(&loader, &arena, project.dependency_names[i],
                               project.dependency_paths[i]);
    if (!dependency ||
        !append_program_declarations(&program, &next_decl, dependency)) {
      had_error = true;
      goto cleanup;
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
  free(loader.programs);
  free(loader.loading);

early_cleanup:
  for (size_t i = 0; i < module_path_count; i++)
    free(module_paths[i]);
  free(module_paths);
  free(stdlib_root);
  free(discovered_manifest);
  runes_project_destroy(&project);
  free(config.filenames);
  free(config.module_paths);

  return had_error ? 1 : 0;
}
