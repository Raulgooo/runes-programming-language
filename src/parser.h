#ifndef RUNES_PARSER_H
#define RUNES_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdbool.h>

typedef struct {
  Lexer *lexer;
  Arena *arena;
  Token current;
  Token next;
  const char *filename;
  const char *source;
  bool had_error;
  bool panic_mode;
  int error_count;
} Parser;

void parser_init(Parser *p, Lexer *lexer, Arena *arena, const char *filename,
                 const char *source);
AstNode *parser_parse(Parser *p);
void parser_free(Parser *p);

#endif // RUNES_PARSER_H
