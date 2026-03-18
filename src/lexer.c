#include "lexer.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// ── init
// ──────────────────────────────────────────────────────────────────────
void lexer_init(Lexer *L, const char *source, StrTab *strtab) {
  L->source = source;
  L->current = source;
  L->start = source;
  L->line = 1;
  L->column = 1;
  L->start_column = 1;
  L->strtab = strtab;
}

// ── helpers
// ───────────────────────────────────────────────────────────────────
static bool is_at_end(Lexer *L) { return *L->current == '\0'; }

static char peek(Lexer *L) { return *L->current; }

static char peek_next(Lexer *L) {
  if (is_at_end(L))
    return '\0';
  return *(L->current + 1);
}

static char peek_after_next(Lexer *L) {
  if (is_at_end(L) || *(L->current + 1) == '\0')
    return '\0';
  return *(L->current + 2);
}

static char advance(Lexer *L) {
  char c = *L->current++;
  if (c == '\n') {
    L->line++;
    L->column = 1;
  } else {
    L->column++;
  }
  return c;
}

static bool match(Lexer *L, char expected) {
  if (is_at_end(L))
    return false;
  if (peek(L) != expected)
    return false;
  advance(L);
  return true;
}

// make_token — does NOT set union fields; caller must fill them in if needed.
static Token make_token(Lexer *L, TokenKind kind) {
  Token token;
  memset(&token, 0, sizeof(Token));
  token.kind = kind;
  token.start = L->start;
  token.length = (size_t)(L->current - L->start);
  token.line = L->line;
  token.column = L->column - (int)token.length;
  return token;
}

static Token error_token(Lexer *L) {
  Token token;
  memset(&token, 0, sizeof(Token));
  token.kind = TOKEN_INVALID;
  token.start = L->start;
  token.length = (size_t)(L->current - L->start);
  token.line = L->line;
  token.column = L->column - (int)token.length;
  return token;
}

static void skip_whitespace_and_comments(Lexer *L) {
  for (;;) {
    char c = peek(L);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
    case '\n':
      advance(L);
      break;
    case '-':
      if (peek_next(L) == '-') {
        if (peek_after_next(L) == '-') {
          // Multiline: --- ... ---
          advance(L);
          advance(L);
          advance(L);
          while (!is_at_end(L)) {
            if (peek(L) == '-' && peek_next(L) == '-' &&
                peek_after_next(L) == '-') {
              advance(L);
              advance(L);
              advance(L);
              break;
            }
            advance(L);
          }
        } else {
          // Single line: --
          while (!is_at_end(L) && peek(L) != '\n') {
            advance(L);
          }
        }
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_hex_digit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// ── keyword trie ─────────────────────────────────────────────────────────────
static TokenKind check_keyword(Lexer *L, int start, int length,
                               const char *rest, TokenKind kind) {
  if (L->current - L->start == start + length &&
      memcmp(L->start + start, rest, (size_t)length) == 0) {
    return kind;
  }
  return TOKEN_IDENTIFIER;
}

static TokenKind identifier_kind(Lexer *L) {
  switch (L->start[0]) {
  case 'a':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'n':
        return check_keyword(L, 2, 1, "d", TOKEN_AND);
      case 's':
        if (L->current - L->start == 2)
          return TOKEN_AS;
        return check_keyword(L, 2, 1, "m", TOKEN_ASM);
      case 'l':
        return check_keyword(L, 2, 5, "ignof", TOKEN_ALIGNOF);
      }
    }
    break;
  case 'o':
    return check_keyword(L, 1, 1, "r", TOKEN_OR);
  case 'b':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'o':
        return check_keyword(L, 2, 2, "ol", TOKEN_BOOL);
      case 'r':
        return check_keyword(L, 2, 3, "eak", TOKEN_BREAK);
      }
    }
    break;
  case 'c':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'a':
        return check_keyword(L, 2, 3, "tch", TOKEN_CATCH);
      case 'h':
        return check_keyword(L, 2, 2, "ar", TOKEN_CHAR);
      case 'o':
        if (L->current - L->start > 2) {
          switch (L->start[2]) {
          case 'n':
            if (L->current - L->start > 3) {
              switch (L->start[3]) {
              case 's':
                return check_keyword(L, 4, 1, "t", TOKEN_CONST);
              case 't':
                return check_keyword(L, 4, 4, "inue", TOKEN_CONTINUE);
              }
            }
            break;
          }
        }
        break;
      }
    }
    break;
  case 'd':
    return check_keyword(L, 1, 6, "ynamic", TOKEN_DYNAMIC);
  case 'e':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'l':
        return check_keyword(L, 2, 2, "se", TOKEN_ELSE);
      case 'r':
        return check_keyword(L, 2, 3, "ror", TOKEN_ERROR);
      case 'x':
        return check_keyword(L, 2, 4, "tern", TOKEN_EXTERN);
      }
    }
    break;
  case 'f':
    if (L->current - L->start == 1)
      return TOKEN_F;
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'a':
        return check_keyword(L, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return check_keyword(L, 2, 1, "r", TOKEN_FOR);
      case 'l':
        return check_keyword(L, 2, 2, "ex", TOKEN_FLEX);
      case '3':
        return check_keyword(L, 2, 1, "2", TOKEN_F32);
      case '6':
        return check_keyword(L, 2, 1, "4", TOKEN_F64);
      }
    }
    break;
  case 'g':
    return check_keyword(L, 1, 1, "c", TOKEN_GC);
  case 'i':
    if (L->current - L->start == 2) {
      switch (L->start[1]) {
      case 'f':
        return TOKEN_IF;
      case '8':
        return TOKEN_I8;
      }
    }
    if (L->current - L->start == 3) {
      if (L->start[1] == '1' && L->start[2] == '6')
        return TOKEN_I16;
      if (L->start[1] == '3' && L->start[2] == '2')
        return TOKEN_I32;
      if (L->start[1] == '6' && L->start[2] == '4')
        return TOKEN_I64;
    }
    if (L->current - L->start == 9)
      return check_keyword(L, 1, 8, "nterface", TOKEN_INTERFACE);
    return TOKEN_IDENTIFIER;
  case 'J':
    if (L->current - L->start == 1)
      return TOKEN_J;
    break;
  case 'l':
    return check_keyword(L, 1, 3, "oop", TOKEN_LOOP);
  case 'm':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'a':
        return check_keyword(L, 2, 3, "tch", TOKEN_MATCH);
      case 'e':
        return check_keyword(L, 2, 4, "thod", TOKEN_METHOD);
      case 'o':
        return check_keyword(L, 2, 1, "d", TOKEN_MOD);
      }
    }
    break;
  case 'p':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'u':
        return check_keyword(L, 2, 1, "b", TOKEN_PUB);
      case 'r':
        return check_keyword(L, 2, 5, "omote", TOKEN_PROMOTE);
      }
    }
    break;
  case 'r':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'e':
        if (L->current - L->start > 2) {
          switch (L->start[2]) {
          case 'g':
            return check_keyword(L, 3, 5, "ional", TOKEN_REGIONAL);
          case 't':
            return check_keyword(L, 3, 3, "urn", TOKEN_RETURN);
          }
        }
        break;
      }
    }
    break;
  case 's':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'c':
        return check_keyword(L, 2, 4, "hema", TOKEN_SCHEMA);
      case 'e':
        return check_keyword(L, 2, 2, "lf", TOKEN_SELF);
      case 'i':
        return check_keyword(L, 2, 4, "zeof", TOKEN_SIZEOF);
      case 't':
        if (L->current - L->start > 2) {
          switch (L->start[2]) {
          case 'r':
            return check_keyword(L, 3, 0, "", TOKEN_STR);
          case 'a':
            return check_keyword(L, 3, 2, "ck", TOKEN_STACK);
          }
        }
        break;
      }
    }
    break;
  case 't':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'r':
        if (L->current - L->start == 3 && L->start[2] == 'y')
          return TOKEN_TRY;
        return check_keyword(L, 2, 2, "ue", TOKEN_TRUE);
      case 'y':
        return check_keyword(L, 2, 2, "pe", TOKEN_TYPE);
      }
    }
    break;
  case 'u':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 's':
        if (L->current - L->start == 3)
          return check_keyword(L, 2, 1, "e", TOKEN_USE);
        return check_keyword(L, 2, 3, "ize", TOKEN_USIZE);
      case 'n':
        return check_keyword(L, 2, 4, "safe", TOKEN_UNSAFE);
      case '8':
        if (L->current - L->start == 2)
          return TOKEN_U8;
        break;
      }
    }
    if (L->current - L->start == 3) {
      if (L->start[1] == '1' && L->start[2] == '6')
        return TOKEN_U16;
      if (L->start[1] == '3' && L->start[2] == '2')
        return TOKEN_U32;
      if (L->start[1] == '6' && L->start[2] == '4')
        return TOKEN_U64;
    }
    break;
  case 'v':
    if (L->current - L->start > 1) {
      switch (L->start[1]) {
      case 'o':
        if (L->current - L->start > 2) {
          switch (L->start[2]) {
          case 'i':
            return check_keyword(L, 3, 1, "d", TOKEN_VOID);
          case 'l':
            return check_keyword(L, 3, 5, "atile", TOKEN_VOLATILE);
          }
        }
        break;
      }
    }
    break;
  case 'w':
    return check_keyword(L, 1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

// ── literal scanners ─────────────────────────────────────────────────────────

static Token identifier(Lexer *L) {
  while (is_alpha(peek(L)) || is_digit(peek(L)))
    advance(L);

  TokenKind kind = identifier_kind(L);
  Token tok = make_token(L, kind);

  // Intern all identifier-like tokens so str_val.ptr is always valid
  if (L->strtab) {
    tok.str_val.ptr =
        strtab_intern(L->strtab, L->start, (size_t)(L->current - L->start));
    tok.str_val.len = (size_t)(L->current - L->start);
  }
  return tok;
}

static Token number(Lexer *L) {
  Token tok;
  bool is_hex = false;

  if (L->start[0] == '0' && (peek(L) == 'x' || peek(L) == 'X')) {
    if (!is_hex_digit(peek_next(L))) {
      // '0' alone — 'x' is NOT part of the literal; return just '0'
      tok = make_token(L, TOKEN_INT_LITERAL);
      tok.int_val = 0;
      return tok;
    }
    advance(L); // consume 'x'/'X'
    while (!is_at_end(L) && is_hex_digit(peek(L)))
      advance(L);
    is_hex = true;
    tok = make_token(L, TOKEN_INT_LITERAL);

    char buf[64];
    size_t len = (size_t)(L->current - L->start);
    if (len >= sizeof(buf))
      len = sizeof(buf) - 1;
    memcpy(buf, L->start, len);
    buf[len] = '\0';
    tok.int_val = (int64_t)strtoull(buf, NULL, 16);
    return tok;
  }

  // Decimal digits
  while (is_digit(peek(L)))
    advance(L);

  // Fractional part?
  if (peek(L) == '.' && is_digit(peek_next(L))) {
    advance(L); // consume '.'
    while (is_digit(peek(L)))
      advance(L);
    tok = make_token(L, TOKEN_FLOAT_LITERAL);
    char buf[64];
    size_t len = (size_t)(L->current - L->start);
    if (len >= sizeof(buf))
      len = sizeof(buf) - 1;
    memcpy(buf, L->start, len);
    buf[len] = '\0';
    tok.float_val = strtod(buf, NULL);
    return tok;
  }

  tok = make_token(L, TOKEN_INT_LITERAL);
  char buf[64];
  size_t len = (size_t)(L->current - L->start);
  if (len >= sizeof(buf))
    len = sizeof(buf) - 1;
  memcpy(buf, L->start, len);
  buf[len] = '\0';
  tok.int_val = (uint64_t)strtoull(buf, NULL, 0);
  (void)is_hex;
  return tok;
}

// Decode a single escape sequence; *p advances past the escape.
// Returns the decoded byte value (simple ASCII) or the first byte of a UTF-8
// sequence. For full Unicode (\uXXXX) only the low byte is returned for now.
static uint32_t decode_escape(const char **p) {
  char c = *(*p)++;
  switch (c) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '\"':
    return '\"';
  case '0':
    return '\0';
  case 'u': {
    // \uXXXX  — exactly 4 hex digits
    uint32_t cp = 0;
    for (int i = 0; i < 4; i++) {
      if (!**p)
        break;
      char h = *(*p)++;
      if (h >= '0' && h <= '9')
        cp = cp * 16 + (uint32_t)(h - '0');
      else if (h >= 'a' && h <= 'f')
        cp = cp * 16 + (uint32_t)(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F')
        cp = cp * 16 + (uint32_t)(h - 'A' + 10);
      else {
        --(*p);
        break;
      }
    }
    return cp;
  }
  default:
    return (uint32_t)(unsigned char)c;
  }
}

static Token string_lit(Lexer *L) {
  // L->start points at the opening '"'; current is one past it
  while (peek(L) != '"' && !is_at_end(L)) {
    if (peek(L) == '\n') {
      L->line++;
      L->column = 1;
    }
    if (peek(L) == '\\')
      advance(L); // skip backslash; next advance handles the escaped char
    advance(L);
  }

  if (is_at_end(L))
    return error_token(L);

  advance(L); // closing '"'
  Token tok = make_token(L, TOKEN_STRING_LITERAL);

  // Intern the string content (excluding quotes)
  if (L->strtab) {
    const char *inner = L->start + 1;                       // skip opening '"'
    size_t inner_len = (size_t)(L->current - L->start - 2); // strip both '"'
    tok.str_val.ptr = strtab_intern(L->strtab, inner, inner_len);
    tok.str_val.len = inner_len;
  }
  return tok;
}

static Token char_literal(Lexer *L) {
  // L->start points at the opening '\''; L->current is already one past it
  uint32_t cp;

  if (peek(L) == '\\') {
    advance(L); // consume backslash
    const char *p = L->current;
    cp = decode_escape(&p);
    // advance L->current to where p ended up
    while (L->current < p)
      advance(L);
  } else {
    cp = (uint32_t)(unsigned char)advance(L);
  }

  if (peek(L) != '\'')
    return error_token(L);

  advance(L); // closing '\''
  Token tok = make_token(L, TOKEN_CHAR_LITERAL);
  tok.char_val = cp;
  return tok;
}

// ── main dispatch
// ─────────────────────────────────────────────────────────────
static Token token_next(Lexer *L) {
  skip_whitespace_and_comments(L);
  L->start = L->current;
  L->start_column = L->column;

  if (is_at_end(L))
    return make_token(L, TOKEN_EOF);

  char c = advance(L);

  if (is_alpha(c))
    return identifier(L);
  if (is_digit(c))
    return number(L);

  switch (c) {
  case '\n':
    return make_token(L, TOKEN_NEWLINE);
  case '(':
    return make_token(L, TOKEN_LPAREN);
  case ')':
    return make_token(L, TOKEN_RPAREN);
  case '{':
    return make_token(L, TOKEN_LBRACE);
  case '}':
    return make_token(L, TOKEN_RBRACE);
  case '[':
    return make_token(L, TOKEN_LBRACKET);
  case ']':
    return make_token(L, TOKEN_RBRACKET);
  case ',':
    return make_token(L, TOKEN_COMMA);
  case ':':
    if (match(L, '='))
      return make_token(L, TOKEN_COLON_EQUAL);
    return make_token(L, TOKEN_COLON);
  case ';':
    return make_token(L, TOKEN_SEMICOLON);
  case '|':
    return make_token(L, TOKEN_PIPE);
  case '#':
    return make_token(L, TOKEN_HASH);
  case '~':
    return make_token(L, TOKEN_TILDE);
  case '^':
    return make_token(L, TOKEN_CARET);
  case '%':
    return make_token(L, TOKEN_PERCENT);
  case '+':
    return make_token(L, TOKEN_PLUS);
  case '*':
    return make_token(L, TOKEN_STAR);
  case '/':
    return make_token(L, TOKEN_SLASH);

  case '.':
    if (match(L, '.')) {
      if (match(L, '='))
        return make_token(L, TOKEN_RANGE_INC);
      return make_token(L, TOKEN_RANGE);
    }
    return make_token(L, TOKEN_DOT);

  case '-':
    if (match(L, '>'))
      return make_token(L, TOKEN_ARROW);
    return make_token(L, TOKEN_MINUS);

  case '=':
    if (match(L, '='))
      return make_token(L, TOKEN_EQ_EQ);
    return make_token(L, TOKEN_EQUAL);

  case '!':
    if (match(L, '='))
      return make_token(L, TOKEN_BANG_EQ);
    return make_token(L, TOKEN_BANG);

  case '<':
    if (match(L, '='))
      return make_token(L, TOKEN_LT_EQ);
    if (match(L, '<'))
      return make_token(L, TOKEN_SHL);
    return make_token(L, TOKEN_LT);

  case '>':
    if (match(L, '='))
      return make_token(L, TOKEN_GT_EQ);
    if (match(L, '>'))
      return make_token(L, TOKEN_SHR);
    return make_token(L, TOKEN_GT);

  case '&':
    return make_token(L, TOKEN_AMP);

  case '"':
    return string_lit(L);
  case '\'':
    return char_literal(L);
  }

  return error_token(L);
}

Token lexer_next_token(Lexer *L) { return token_next(L); }

// ── token_kind_to_string
// ──────────────────────────────────────────────────────
const char *token_kind_to_string(TokenKind kind) {
  switch (kind) {
  case TOKEN_EOF:
    return "EOF";
  case TOKEN_INVALID:
    return "INVALID";
  case TOKEN_IDENTIFIER:
    return "IDENTIFIER";
  case TOKEN_INT_LITERAL:
    return "INT_LITERAL";
  case TOKEN_FLOAT_LITERAL:
    return "FLOAT_LITERAL";
  case TOKEN_STRING_LITERAL:
    return "STRING_LITERAL";
  case TOKEN_CHAR_LITERAL:
    return "CHAR_LITERAL";
  case TOKEN_F:
    return "f";
  case TOKEN_DYNAMIC:
    return "dynamic";
  case TOKEN_REGIONAL:
    return "regional";
  case TOKEN_GC:
    return "gc";
  case TOKEN_FLEX:
    return "flex";
  case TOKEN_STACK:
    return "stack";
  case TOKEN_METHOD:
    return "method";
  case TOKEN_INTERFACE:
    return "interface";
  case TOKEN_TYPE:
    return "type";
  case TOKEN_ERROR:
    return "error";
  case TOKEN_MOD:
    return "mod";
  case TOKEN_USE:
    return "use";
  case TOKEN_PUB:
    return "pub";
  case TOKEN_CONST:
    return "const";
  case TOKEN_MATCH:
    return "match";
  case TOKEN_IF:
    return "if";
  case TOKEN_ELSE:
    return "else";
  case TOKEN_FOR:
    return "for";
  case TOKEN_WHILE:
    return "while";
  case TOKEN_LOOP:
    return "loop";
  case TOKEN_BREAK:
    return "break";
  case TOKEN_CONTINUE:
    return "continue";
  case TOKEN_RETURN:
    return "return";
  case TOKEN_TRY:
    return "try";
  case TOKEN_CATCH:
    return "catch";
  case TOKEN_UNSAFE:
    return "unsafe";
  case TOKEN_ASM:
    return "asm";
  case TOKEN_EXTERN:
    return "extern";
  case TOKEN_VOLATILE:
    return "volatile";
  case TOKEN_PROMOTE:
    return "promote";
  case TOKEN_SIZEOF:
    return "sizeof";
  case TOKEN_ALIGNOF:
    return "alignof";
  case TOKEN_SELF:
    return "self";
  case TOKEN_AS:
    return "as";
  case TOKEN_TRUE:
    return "true";
  case TOKEN_FALSE:
    return "false";
  case TOKEN_J:
    return "J";
  case TOKEN_SCHEMA:
    return "schema";
  case TOKEN_I8:
    return "i8";
  case TOKEN_I16:
    return "i16";
  case TOKEN_I32:
    return "i32";
  case TOKEN_I64:
    return "i64";
  case TOKEN_U8:
    return "u8";
  case TOKEN_U16:
    return "u16";
  case TOKEN_U32:
    return "u32";
  case TOKEN_U64:
    return "u64";
  case TOKEN_F32:
    return "f32";
  case TOKEN_F64:
    return "f64";
  case TOKEN_BOOL:
    return "bool";
  case TOKEN_STR:
    return "str";
  case TOKEN_CHAR:
    return "char";
  case TOKEN_USIZE:
    return "usize";
  case TOKEN_VOID:
    return "void";
  case TOKEN_OR:
    return "or";
  case TOKEN_AND:
    return "and";
  case TOKEN_LPAREN:
    return "(";
  case TOKEN_RPAREN:
    return ")";
  case TOKEN_LBRACE:
    return "{";
  case TOKEN_RBRACE:
    return "}";
  case TOKEN_LBRACKET:
    return "[";
  case TOKEN_RBRACKET:
    return "]";
  case TOKEN_COMMA:
    return ",";
  case TOKEN_DOT:
    return ".";
  case TOKEN_COLON:
    return ":";
  case TOKEN_COLON_EQUAL:
    return ":=";
  case TOKEN_SEMICOLON:
    return ";";
  case TOKEN_NEWLINE:
    return "NEWLINE";
  case TOKEN_ARROW:
    return "->";
  case TOKEN_RANGE:
    return "..";
  case TOKEN_RANGE_INC:
    return "..=";
  case TOKEN_PIPE:
    return "|";
  case TOKEN_HASH:
    return "#";
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_STAR:
    return "*";
  case TOKEN_SLASH:
    return "/";
  case TOKEN_PERCENT:
    return "%";
  case TOKEN_EQUAL:
    return "=";
  case TOKEN_EQ_EQ:
    return "==";
  case TOKEN_BANG_EQ:
    return "!=";
  case TOKEN_LT:
    return "<";
  case TOKEN_LT_EQ:
    return "<=";
  case TOKEN_GT:
    return ">";
  case TOKEN_GT_EQ:
    return ">=";
  case TOKEN_BANG:
    return "!";
  case TOKEN_AMP:
    return "&";
  case TOKEN_CARET:
    return "^";
  case TOKEN_TILDE:
    return "~";
  case TOKEN_SHL:
    return "<<";
  case TOKEN_SHR:
    return ">>";
  }
  return "UNKNOWN";
}
