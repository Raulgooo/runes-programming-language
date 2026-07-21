#include "../codegen.h"
#include "../lexer.h"
#include "../monomorphize.h"
#include "../parser.h"
#include "../resolver.h"
#include "../symbol_table.h"
#include "../typecheck.h"
#include "../types.h"
#include "../utils/arena.h"
#include "../utils/strtab.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > 64 * 1024)
    return 0;

  char *source = malloc(size + 1);
  if (!source)
    return 0;
  memcpy(source, data, size);
  source[size] = '\0';

  Arena arena;
  if (!arena_init(&arena)) {
    free(source);
    return 0;
  }

  StrTab strtab;
  strtab_init(&strtab, &arena);

  Lexer lexer;
  lexer_init(&lexer, source, &strtab);

  Parser parser;
  parser_init(&parser, &lexer, &arena, "<fuzz>", source);
  AstNode *program = parser_parse(&parser);

  if (program && !parser.had_error && monomorphize_program(&arena, program)) {
    SymbolTable symbols;
    symbol_table_init(&symbols, &arena);

    Resolver resolver;
    resolver_init(&resolver, &symbols);
    resolver_resolve(&resolver, program);

    if (!resolver.had_error) {
      TypeContext types;
      type_context_init(&types, &arena);

      TypeChecker checker;
      typechecker_init(&checker, &arena, &types, &symbols);
      typechecker_check(&checker, program);

      if (!checker.had_error) {
        FILE *output = tmpfile();
        if (output) {
          Codegen codegen;
          codegen_init(&codegen, output, &arena);
          (void)codegen_emit_c(&codegen, program);
          fclose(output);
        }
      }
    }
  }

  arena_destroy(&arena);
  free(source);
  return 0;
}
