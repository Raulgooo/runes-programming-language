#ifndef RUNES_LEXER_H
#define RUNES_LEXER_H

#include "utils/strtab.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  TOKEN_EOF = 0,
  TOKEN_INVALID,

  /* Literals */
  TOKEN_IDENTIFIER,
  TOKEN_INT_LITERAL,
  TOKEN_FLOAT_LITERAL,
  TOKEN_STRING_LITERAL,
  TOKEN_CHAR_LITERAL,

  /* Keywords */
  TOKEN_F,
  TOKEN_DYNAMIC,
  TOKEN_REGIONAL,
  TOKEN_GC,
  TOKEN_FLEX,
  TOKEN_STACK,
  TOKEN_METHOD,
  TOKEN_INTERFACE,
  TOKEN_TYPE,
  TOKEN_ERROR,
  TOKEN_MOD,
  TOKEN_USE,
  TOKEN_PUB,
  TOKEN_CONST,
  TOKEN_MATCH,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_FOR,
  TOKEN_WHILE,
  TOKEN_LOOP,
  TOKEN_BREAK,
  TOKEN_CONTINUE,
  TOKEN_RETURN,
  TOKEN_TRY,
  TOKEN_CATCH,
  TOKEN_UNSAFE,
  TOKEN_ASM,
  TOKEN_EXTERN,
  TOKEN_VOLATILE,
  TOKEN_PROMOTE,
  TOKEN_SIZEOF,
  TOKEN_ALIGNOF,
  TOKEN_SELF,
  TOKEN_AS,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_J,
  TOKEN_SCHEMA,

  /* Primitive Types */
  TOKEN_I8,
  TOKEN_I16,
  TOKEN_I32,
  TOKEN_I64,
  TOKEN_U8,
  TOKEN_U16,
  TOKEN_U32,
  TOKEN_U64,
  TOKEN_F32,
  TOKEN_F64,
  TOKEN_BOOL,
  TOKEN_STR,
  TOKEN_CHAR,
  TOKEN_USIZE,
  TOKEN_VOID,
  TOKEN_OR,
  TOKEN_AND,

  /* Punctuation */
  TOKEN_LPAREN,    /* ( */
  TOKEN_RPAREN,    /* ) */
  TOKEN_LBRACE,    /* { */
  TOKEN_RBRACE,    /* } */
  TOKEN_LBRACKET,  /* [ */
  TOKEN_RBRACKET,  /* ] */
  TOKEN_COMMA,     /* , */
  TOKEN_DOT,       /* . */
  TOKEN_COLON,     /* : */
  TOKEN_SEMICOLON, /* ; */
  TOKEN_NEWLINE,   /* \n */
  TOKEN_ARROW,     /* -> */
  TOKEN_RANGE,     /* .. */
  TOKEN_RANGE_INC, /* ..= */
  TOKEN_PIPE,      /* | */
  TOKEN_HASH,      /* # */

  /* Operators */
  TOKEN_PLUS,    /* + */
  TOKEN_MINUS,   /* - */
  TOKEN_STAR,    /* * */
  TOKEN_SLASH,   /* / */
  TOKEN_PERCENT, /* % */
  TOKEN_EQUAL,   /* = */
  TOKEN_EQ_EQ,   /* == */
  TOKEN_BANG_EQ, /* != */
  TOKEN_LT,      /* < */
  TOKEN_LT_EQ,   /* <= */
  TOKEN_GT,      /* > */
  TOKEN_GT_EQ,   /* >= */
  TOKEN_BANG,    /* ! */
  TOKEN_AMP,     /* & */
  TOKEN_CARET,   /* ^ */
  TOKEN_TILDE,   /* ~ */
  TOKEN_SHL,     /* << */
  TOKEN_SHR,     /* >> */
} TokenKind;

typedef struct {
  TokenKind kind;
  const char *start; /* pointer into source text */
  size_t length;     /* byte length of the raw lexeme */
  int line;
  int column;
  union {
    uint64_t int_val;  /* TOKEN_INT_LITERAL */
    double float_val;  /* TOKEN_FLOAT_LITERAL */
    uint32_t char_val; /* TOKEN_CHAR_LITERAL — Unicode codepoint */
    struct {
      const char *ptr; /* interned, NUL-terminated, arena-owned */
      size_t len;      /* byte length (not counting NUL) */
    } str_val;         /* TOKEN_STRING_LITERAL, TOKEN_IDENTIFIER */
  };
} Token;

typedef struct {
  const char *source;
  const char *current;
  const char *start;
  int line;
  int column;
  int start_column;
  StrTab *strtab; /* string-interning table, NULL → no interning */
} Lexer;

/* Pass strtab=NULL if you do not need interned identifiers/strings */
void lexer_init(Lexer *l, const char *source, StrTab *strtab);
Token lexer_next_token(Lexer *l);

const char *token_kind_to_string(TokenKind kind);

#endif /* RUNES_LEXER_H */