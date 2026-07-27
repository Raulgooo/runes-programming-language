#ifndef RUNES_PARSER_H
#define RUNES_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdbool.h>

typedef void (*ParserDiagnosticHandler)(void *context, const char *filename,
                                        uint32_t line, uint32_t column,
                                        const char *message);

typedef struct {
  Lexer *lexer;
  Arena *arena;
  Token current;
  Token next;
  Token next2;
  uint32_t prev_line;
  unsigned soft_delimiter_depth;
  unsigned declaration_depth;
  unsigned function_depth;
  const char *filename;
  const char *source;
  bool had_error;
  bool panic_mode;
  int error_count;
  ParserDiagnosticHandler diagnostic_handler;
  void *diagnostic_context;
} Parser;

void parser_init(Parser *p, Lexer *lexer, Arena *arena, const char *filename,
                 const char *source);
AstNode *parser_parse(Parser *p);
void parser_free(Parser *p);
void parser_set_diagnostic_handler(Parser *p, ParserDiagnosticHandler handler,
                                   void *context);

#endif // RUNES_PARSER_H
