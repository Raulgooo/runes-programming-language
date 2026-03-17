#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Forward declarations
// ──────────────────────────────────────────────────────
static AstNode *parse_expr(Parser *p);
static AstNode *parse_type_expr(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_try(Parser *p);
static AstNode *parse_catch(Parser *p);
static AstNode *parse_arg_list(Parser *p);
static AstNode *parse_decl(Parser *p);
static AstNode *parse_stmt(Parser *p);

// ── Scaffolding
// ───────────────────────────────────────────────────────────────
static Token advance(Parser *p) {
  p->current = p->next;
  p->next = lexer_next_token(p->lexer);
  return p->current;
}

static Token peek(Parser *p) { return p->next; }

static bool check(Parser *p, TokenKind kind) { return p->current.kind == kind; }

static bool match(Parser *p, TokenKind kind) {
  if (check(p, kind)) {
    advance(p);
    return true;
  }
  return false;
}

static void parser_error(Parser *p, const char *msg) {
  fprintf(stderr, "[Error] %s:%d:%d: %s\n", p->filename, p->current.line,
          p->current.column, msg);
  p->had_error = true;
  p->error_count++;
}

static Token expect(Parser *p, TokenKind kind, const char *msg) {
  if (check(p, kind)) {
    return advance(p);
  }
  parser_error(p, msg);
  return (Token){0};
}

static void synchronize(Parser *p) {
  advance(p); // skip the fucked up token

  while (p->current.kind != TOKEN_EOF) {
    switch (p->current.kind) {
    // stop ON these but don't consume
    case TOKEN_RBRACE:
    case TOKEN_RPAREN:
    case TOKEN_RBRACKET:
      return;

    // declaration keywords — safe to resume here
    case TOKEN_F:
    case TOKEN_TYPE:
    case TOKEN_SCHEMA:
    case TOKEN_ERROR:
    case TOKEN_MOD:
    case TOKEN_USE:
    case TOKEN_EXTERN:
    case TOKEN_PUB:
    case TOKEN_REGIONAL:
    case TOKEN_DYNAMIC:
    case TOKEN_GC:
    case TOKEN_FLEX:
    case TOKEN_STACK:
    // statement keywords — safe to resume here
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_FOR:
    case TOKEN_LOOP:
    case TOKEN_MATCH:
    case TOKEN_UNSAFE:
    case TOKEN_RETURN:
      return;

    default:
      advance(p);
      break;
    }
  }
}

void parser_init(Parser *p, Lexer *lexer, Arena *arena, const char *filename,
                 const char *source) {
  p->lexer = lexer;
  p->arena = arena;
  p->filename = filename;
  p->source = source;
  p->had_error = false;
  p->error_count = 0;
  p->current = (Token){0};
  p->next = lexer_next_token(lexer); // prime the buffer
  advance(p);                        // current = first token, next = second
}

void parser_free(Parser *p) { (void)p; }

// ── Type expressions
// ──────────────────────────────────────────────────────────
static AstNode *parse_type_expr(Parser *p) {
  Token T = p->current;

  // pointer: *T
  if (match(p, TOKEN_STAR)) {
    AstNode *inner = parse_type_expr(p);
    if (!inner)
      return NULL;
    AstNode *n = ast_new_type_ptr(p->arena, inner);
    n->line = T.line;
    n->col = T.column;
    return n;
  }

  // fallible: !T
  if (match(p, TOKEN_BANG)) {
    AstNode *inner = parse_type_expr(p);
    // inner may be NULL for !void
    AstNode *n = ast_new_type_fallible(p->arena, inner);
    n->line = T.line;
    n->col = T.column;
    return n;
  }

  // array: [N]T
  if (match(p, TOKEN_LBRACKET)) {
    AstNode *size = parse_expr(p);
    if (!size)
      return NULL;
    expect(p, TOKEN_RBRACKET, "expected ']' after array size");
    if (p->had_error)
      return NULL;
    AstNode *elem = parse_type_expr(p);
    if (!elem)
      return NULL;
    AstNode *n = ast_new_type_array(p->arena, size, elem);
    n->line = T.line;
    n->col = T.column;
    return n;
  }

  // tuple: (T, U, ...)
  if (match(p, TOKEN_LPAREN)) {
    AstNode *head = NULL, *tail = NULL;
    if (!check(p, TOKEN_RPAREN)) {
      do {
        AstNode *elem = parse_type_expr(p);
        if (!elem)
          return NULL;
        if (!head) {
          head = tail = elem;
        } else {
          tail->next = elem;
          tail = elem;
        }
      } while (match(p, TOKEN_COMMA));
    }
    expect(p, TOKEN_RPAREN, "expected ')' after tuple type");
    if (p->had_error)
      return NULL;
    AstNode *n = ast_new_type_tuple(p->arena, head);
    n->line = T.line;
    n->col = T.column;
    return n;
  }

  // named primitive / user-defined type
  if (check(p, TOKEN_IDENTIFIER) || check(p, TOKEN_I8) || check(p, TOKEN_I16) ||
      check(p, TOKEN_I32) || check(p, TOKEN_I64) || check(p, TOKEN_U8) ||
      check(p, TOKEN_U16) || check(p, TOKEN_U32) || check(p, TOKEN_U64) ||
      check(p, TOKEN_F32) || check(p, TOKEN_F64) || check(p, TOKEN_BOOL) ||
      check(p, TOKEN_STR) || check(p, TOKEN_CHAR) || check(p, TOKEN_USIZE) ||
      check(p, TOKEN_VOID)) {
    Token name_tok = advance(p);
    const char *name;
    if (name_tok.kind == TOKEN_IDENTIFIER) {
      name = name_tok.str_val.ptr;
    } else {
      name = token_kind_to_string(name_tok.kind);
    }
    AstNode *n = ast_new_type_named(p->arena, name);
    n->line = name_tok.line;
    n->col = name_tok.column;
    return n;
  }

  // J type
  if (check(p, TOKEN_J)) {
    Token j_tok = advance(p);
    AstNode *n = ast_new_type_j(p->arena);
    n->line = j_tok.line;
    n->col = j_tok.column;
    return n;
  }

  parser_error(p, "expected a type expression");
  return NULL;
}

// ── Declarations ─────────────────────────────────────────────────────────────
static AstNode *parse_func_decl(Parser *p, bool is_pub, MemoryRealm realm);
static AstNode *parse_var_decl(Parser *p, bool is_const, bool is_volatile);
static AstNode *parse_type_decl(Parser *p, bool is_pub);
static AstNode *parse_variant_decl(Parser *p, bool is_pub);
static AstNode *parse_schema_decl(Parser *p, bool is_pub);
static AstNode *parse_method_decl(Parser *p, bool is_pub);
static AstNode *parse_interface_decl(Parser *p, bool is_pub);
static AstNode *parse_error_decl(Parser *p, bool is_pub);
static AstNode *parse_mod_decl(Parser *p, bool is_pub);
static AstNode *parse_use_decl(Parser *p);
static AstNode *parse_extern_decl(Parser *p);
static AstNode *parse_param_list(Parser *p);
static AstNode *parse_param(Parser *p);
static Attr *parse_attrs(Parser *p);

// ── Statements
// ────────────────────────────────────────────────────────────────
static AstNode *parse_if_stmt(Parser *p);
static AstNode *parse_while_stmt(Parser *p);
static AstNode *parse_for_stmt(Parser *p);
static AstNode *parse_loop_stmt(Parser *p);
static AstNode *parse_match_stmt(Parser *p);
static AstNode *parse_match_arm(Parser *p);
static AstNode *parse_unsafe_block(Parser *p);

// ── Expressions
// ───────────────────────────────────────────────────────────────
static AstNode *parse_assign(Parser *p) {
  AstNode *left = parse_catch(p); // parse left side first
  if (!left)
    return NULL;

  if (match(p, TOKEN_EQUAL)) {
    Token eq = p->current;
    AstNode *value = parse_assign(p); // recursive — right-associative
    if (!value)
      return NULL;
    AstNode *n = ast_new_assign(p->arena, left, value);
    n->line = eq.line;
    n->col = eq.column;
    return n;
  }

  return left;
}

static AstNode *parse_expr(Parser *p) { return parse_assign(p); }

static AstNode *parse_catch(Parser *p) {
  AstNode *expr = parse_try(p);
  if (!expr)
    return NULL;

  while (check(p, TOKEN_CATCH)) {
    Token catch_tok = advance(p);

    const char *err_name = NULL;
    AstNode *handler = NULL;

    if (match(p, TOKEN_PIPE)) {
      // catch |e| { ... } form
      Token err = expect(p, TOKEN_IDENTIFIER, "expected error name after '|'");
      if (p->had_error)
        return NULL;
      err_name = err.str_val.ptr;
      expect(p, TOKEN_PIPE, "expected '|' after error name");
      if (p->had_error)
        return NULL;
      handler = parse_block(p);
    } else {
      // catch 0.0 — default value form
      handler = parse_expr(p);
    }

    if (!handler)
      return NULL;
    AstNode *n = ast_new_catch_expr(p->arena, expr, err_name, handler);
    n->line = catch_tok.line;
    n->col = catch_tok.column;
    expr = n;
  }

  return expr;
}

static AstNode *parse_param(Parser *p) {
  Token name = expect(p, TOKEN_IDENTIFIER, "expected parameter name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_COLON, "expected ':' after parameter name");
  if (p->had_error)
    return NULL;
  AstNode *type = parse_type_expr(p);
  if (!type)
    return NULL;
  AstNode *n = ast_new_param(p->arena, name.str_val.ptr, type);
  n->line = name.line;
  n->col = name.column;
  return n;
}

static AstNode *parse_param_list(Parser *p) {
  AstNode *head = NULL, *tail = NULL;

  while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
    AstNode *param = parse_param(p);
    if (!param)
      return NULL;
    if (!head) {
      head = tail = param;
    } else {
      tail->next = param;
      tail = param;
    }
    if (!check(p, TOKEN_RPAREN)) {
      expect(p, TOKEN_COMMA, "expected ',' or ')' in parameter list");
      if (p->had_error)
        return NULL;
    }
  }

  return head;
}

static AstNode *parse_primary(Parser *p) {
  if (check(p, TOKEN_INT_LITERAL)) {
    Token tok = advance(p);
    AstNode *n = ast_new_int_literal(p->arena, tok.int_val);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }
  if (check(p, TOKEN_FLOAT_LITERAL)) {
    Token tok = advance(p);
    AstNode *n = ast_new_float_literal(p->arena, tok.float_val);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }
  if (check(p, TOKEN_STRING_LITERAL)) {
    Token tok = advance(p);
    AstNode *n = ast_new_string_literal(p->arena, tok.str_val.ptr);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }
  if (check(p, TOKEN_CHAR_LITERAL)) {
    Token tok = advance(p);
    AstNode *n = ast_new_char_literal(p->arena, tok.char_val);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }
  if (check(p, TOKEN_TRUE)) {
    Token tok = advance(p);
    AstNode *n = ast_new_bool_literal(p->arena, true);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }
  if (check(p, TOKEN_FALSE)) {
    Token tok = advance(p);
    AstNode *n = ast_new_bool_literal(p->arena, false);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }

  // array literal — [1, 2, 3] or [] (zero-initialized)
  if (check(p, TOKEN_LBRACKET)) {
    Token tok = advance(p);
    AstNode *elems = NULL;
    if (!check(p, TOKEN_RBRACKET)) {
      AstNode *head = NULL, *tail = NULL;
      do {
        AstNode *elem = parse_expr(p);
        if (!elem)
          return NULL;
        if (!head) {
          head = tail = elem;
        } else {
          tail->next = elem;
          tail = elem;
        }
      } while (match(p, TOKEN_COMMA));
      elems = head;
    }
    expect(p, TOKEN_RBRACKET, "expected ']' after array literal");
    if (p->had_error)
      return NULL;
    AstNode *n = ast_new_array_literal(p->arena, elems);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }

  // parenthesized expression or tuple — (a) or (a, b)
  if (check(p, TOKEN_LPAREN)) {
    Token tok = advance(p);
    AstNode *first = parse_expr(p);
    if (!first)
      return NULL;
    if (match(p, TOKEN_COMMA)) {
      // tuple
      AstNode *head = first, *tail = first;
      do {
        AstNode *elem = parse_expr(p);
        if (!elem)
          return NULL;
        tail->next = elem;
        tail = elem;
      } while (match(p, TOKEN_COMMA));
      expect(p, TOKEN_RPAREN, "expected ')' after tuple");
      if (p->had_error)
        return NULL;
      AstNode *n = ast_new_tuple_expr(p->arena, head);
      n->line = tok.line;
      n->col = tok.column;
      return n;
    }
    expect(p, TOKEN_RPAREN, "expected ')' after expression");
    if (p->had_error)
      return NULL;
    return first; // grouped expression
  }

  // identifier
  if (check(p, TOKEN_IDENTIFIER)) {
    Token tok = advance(p);
    AstNode *n = ast_new_identifier(p->arena, tok.str_val.ptr);
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }

  parser_error(p, "expected an expression");
  return NULL;
}

static AstNode *parse_postfix(Parser *p) {
  AstNode *left = parse_primary(p);
  if (!left)
    return NULL;

  while (true) {
    if (check(p, TOKEN_LPAREN)) {
      Token open = advance(p);
      AstNode *args = parse_arg_list(p);
      expect(p, TOKEN_RPAREN, "expected ')' after arguments");
      if (p->had_error)
        return NULL;
      AstNode *n = ast_new_call(p->arena, left, args);
      n->line = open.line;
      n->col = open.column;
      left = n;

    } else if (check(p, TOKEN_LBRACKET)) {
      Token open = advance(p);
      AstNode *index = parse_expr(p);
      if (!index)
        return NULL;
      expect(p, TOKEN_RBRACKET, "expected ']' after index");
      if (p->had_error)
        return NULL;
      AstNode *n = ast_new_index(p->arena, left, index);
      n->line = open.line;
      n->col = open.column;
      left = n;

    } else if (check(p, TOKEN_DOT)) {
      Token dot = advance(p);
      Token field =
          expect(p, TOKEN_IDENTIFIER, "expected field name after '.'");
      if (p->had_error)
        return NULL;
      AstNode *n = ast_new_field(p->arena, left, field.str_val.ptr);
      n->line = dot.line;
      n->col = dot.column;
      left = n;

    } else {
      break;
    }
  }
  return left;
}

static AstNode *parse_cast(Parser *p) {
  AstNode *expr = parse_postfix(p);
  if (!expr)
    return NULL;

  while (check(p, TOKEN_AS)) {
    Token as_tok = advance(p);
    AstNode *target = parse_type_expr(p);
    if (!target)
      return NULL;
    AstNode *n = ast_new_cast(p->arena, CAST_RAW, expr, target);
    n->line = as_tok.line;
    n->col = as_tok.column;
    expr = n;
  }
  return expr;
}

static AstNode *parse_unary(Parser *p) {
  if (check(p, TOKEN_BANG) || check(p, TOKEN_MINUS) || check(p, TOKEN_TILDE) ||
      check(p, TOKEN_AMP) || check(p, TOKEN_STAR)) {
    Token op = advance(p);
    AstNode *right = parse_unary(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_unary(p->arena, op.kind, right);
    n->line = op.line;
    n->col = op.column;
    return n;
  }
  return parse_cast(p);
}

static AstNode *parse_multiplicative(Parser *p) {
  AstNode *left = parse_unary(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) ||
         check(p, TOKEN_PERCENT)) {
    Token op = advance(p);
    AstNode *right = parse_unary(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_additive(Parser *p) {
  AstNode *left = parse_multiplicative(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
    Token op = advance(p);
    AstNode *right = parse_multiplicative(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_bitwise(Parser *p) {
  AstNode *left = parse_additive(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_AMP) || check(p, TOKEN_CARET) || check(p, TOKEN_PIPE) ||
         check(p, TOKEN_SHL) || check(p, TOKEN_SHR)) {
    Token op = advance(p);
    AstNode *right = parse_additive(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_comparison(Parser *p) {
  AstNode *left = parse_bitwise(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_LT) || check(p, TOKEN_LT_EQ) || check(p, TOKEN_GT) ||
         check(p, TOKEN_GT_EQ)) {
    Token op = advance(p);
    AstNode *right = parse_bitwise(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_equality(Parser *p) {
  AstNode *left = parse_comparison(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_EQ_EQ) || check(p, TOKEN_BANG_EQ)) {
    Token op = advance(p);
    AstNode *right = parse_comparison(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_logical_and(Parser *p) {
  AstNode *left = parse_equality(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_AND)) {
    Token op = advance(p);
    AstNode *right = parse_equality(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_logical_or(Parser *p) {
  AstNode *left = parse_logical_and(p);
  if (!left)
    return NULL;

  while (check(p, TOKEN_OR)) {
    Token op = advance(p);
    AstNode *right = parse_logical_and(p);
    if (!right)
      return NULL;
    AstNode *n = ast_new_binary(p->arena, op.kind, left, right);
    n->line = op.line;
    n->col = op.column;
    left = n;
  }
  return left;
}

static AstNode *parse_try(Parser *p) {
  if (check(p, TOKEN_TRY)) {
    Token try_tok = advance(p);
    AstNode *expr = parse_try(p); // recursive — handles try try foo()
    if (!expr)
      return NULL;
    AstNode *n = ast_new_try_expr(p->arena, expr);
    n->line = try_tok.line;
    n->col = try_tok.column;
    return n;
  }

  return parse_logical_or(p);
}

static AstNode *parse_arg_list(Parser *p) {
  AstNode *head = NULL, *tail = NULL;

  while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
    AstNode *arg = parse_expr(p);
    if (!arg)
      return NULL;
    if (!head) {
      head = tail = arg;
    } else {
      tail->next = arg;
      tail = arg;
    }
    if (!check(p, TOKEN_RPAREN)) {
      expect(p, TOKEN_COMMA, "expected ',' or ')' in argument list");
      if (p->had_error)
        return NULL;
    }
  }

  return head; // NULL means empty arg list — that's fine
}