#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Forward declarations
// ──────────────────────────────────────────────────────
// ──  Expressions
static AstNode *parse_expr(Parser *p);
static AstNode *parse_type_expr(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_try(Parser *p);
static AstNode *parse_catch(Parser *p);
static AstNode *parse_arg_list(Parser *p);
static AstNode *parse_additive(Parser *p);
static AstNode *parse_decl(Parser *p);
static AstNode *parse_stmt(Parser *p);
static AstNode *parse_func_decl(Parser *p, bool is_pub, MemoryRealm realm, Attr *attrs);
static AstNode *parse_var_decl(Parser *p, bool is_const, bool is_volatile);
static AstNode *parse_type_decl(Parser *p, bool is_pub, Attr *attrs);
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
static bool is_type_keyword(TokenKind kind);

// ── Statements
// ────────────────────────────────────────────────────────────────
static AstNode *parse_if_stmt(Parser *p);
static AstNode *parse_while_stmt(Parser *p);
static AstNode *parse_for_stmt(Parser *p);
static AstNode *parse_loop_stmt(Parser *p);
static AstNode *parse_match_stmt(Parser *p);
static AstNode *parse_match_arm(Parser *p);
static AstNode *parse_unsafe_block(Parser *p);

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
    case TOKEN_VOLATILE:
    case TOKEN_CONST:
    case TOKEN_I8:
    case TOKEN_I16:
    case TOKEN_I32:
    case TOKEN_I64:
    case TOKEN_U8:
    case TOKEN_U16:
    case TOKEN_U32:
    case TOKEN_U64:
    case TOKEN_F32:
    case TOKEN_F64:
    case TOKEN_BOOL:
    case TOKEN_STR:
    case TOKEN_CHAR:
    case TOKEN_USIZE:
    case TOKEN_VOID:
    case TOKEN_STAR:    // *u32 ...
    case TOKEN_LBRACKET: // [5]i32 ...
    case TOKEN_LPAREN:   // (i32, i32) ...
    case TOKEN_BANG:     // !void ...
    // statement keywords — safe to resume here
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_FOR:
    case TOKEN_LOOP:
    case TOKEN_MATCH:
    case TOKEN_UNSAFE:
    case TOKEN_RETURN:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
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
static AstNode *parse_func_decl(Parser *p, bool is_pub, MemoryRealm realm, Attr *attrs) {
  // caller verified current == TOKEN_F
  Token f_tok = advance(p); // consume 'f'

  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected function name");
  if (p->had_error)
    return NULL;

  bool is_main = (name_tok.str_val.ptr && strcmp(name_tok.str_val.ptr, "main") == 0);

  // parameter list — f name(params)
  expect(p, TOKEN_LPAREN, "expected '(' after function name");
  if (p->had_error)
    return NULL;
  AstNode *params = parse_param_list(p);
  expect(p, TOKEN_RPAREN, "expected ')' after parameters");
  if (p->had_error)
    return NULL;

  // optional named return: = name: type
  //   f add(x: i32, y: i32) = result: i32 { ... }
  //   f greet(name: str) { ... }         ← void, no return clause
  const char *ret_name = NULL;
  AstNode *ret_type = NULL;
  bool has_return = false;
  if (match(p, TOKEN_EQUAL)) {
    has_return = true;
    Token ret_name_tok =
        expect(p, TOKEN_IDENTIFIER, "expected return value name");
    if (p->had_error)
      return NULL;
    ret_name = ret_name_tok.str_val.ptr;
    expect(p, TOKEN_COLON, "expected ':' after return name");
    if (p->had_error)
      return NULL;
    ret_type = parse_type_expr(p);
    if (!ret_type)
      return NULL;
  }

  // body:
  //   Block form:     f foo() = r: i32 { r = 42 }
  //   One-liner:      f square(x: i32) = r: i32  r = x * x
  //   Void block:     f greet() { print("hi") }
  //   Signature-only: body == NULL (interface / extern)
  AstNode *body = NULL;
  if (check(p, TOKEN_LBRACE)) {
    body = parse_block(p);
    if (!body)
      return NULL;
  } else if (has_return) {
    // one-liner — only valid when there IS a named return
    AstNode *expr = parse_expr(p);
    if (!expr)
      return NULL;
    body = ast_new_block(p->arena, expr);
    body->line = expr->line;
    body->col = expr->col;
  }
  // else: no body — interface signature or error recovery

  AstNode *n =
      ast_new_func_decl(p->arena, realm, is_pub, is_main, name_tok.str_val.ptr,
                        params, ret_name, ret_type, body, attrs);
  n->line = f_tok.line;
  n->col = f_tok.column;
  return n;
}

// Spec §2: [const] [volatile] Type name = init
//   i32 x = 42 / const i32 MAX = 512 / volatile *u32 uart = 0x10000000
static AstNode *parse_var_decl(Parser *p, bool is_const, bool is_volatile) {
  Token start_tok = p->current;
  AstNode *type = NULL;
  Token name_tok = {0};
  AstNode *init = NULL;

  // We are already past 'const' and 'volatile' if they were matched by caller.
  // BUT we need to check if we have a type or if it's inferred.
  
  bool has_type = false;
  if (is_type_keyword(p->current.kind) ||
      (check(p, TOKEN_IDENTIFIER) && peek(p).kind == TOKEN_IDENTIFIER) ||
      check(p, TOKEN_STAR) || check(p, TOKEN_LBRACKET) ||
      check(p, TOKEN_BANG) || check(p, TOKEN_LPAREN)) {
    has_type = true;
  }

  if (has_type) {
    type = parse_type_expr(p);
    if (!type) return NULL;
    name_tok = expect(p, TOKEN_IDENTIFIER, "expected variable name");
  } else {
    // Inferred type: const LIMIT = 1024
    name_tok = expect(p, TOKEN_IDENTIFIER, "expected variable name");
    if (p->had_error)
      return NULL;
  }

  AstNode *init = NULL;
  if (match(p, TOKEN_EQUAL)) {
    init = parse_expr(p);
    if (!init)
      return NULL;
  }

  AstNode *n = ast_new_var_decl(p->arena, is_const, is_volatile,
                                name_tok.str_val.ptr, type, init);
  n->line = name_tok.line;
  n->col = name_tok.column;
  return n;
}

// Spec §7: type Vec2 = { x: f32, y: f32 } or type Vec2 = x: f32, y: f32
static AstNode *parse_type_decl(Parser *p, bool is_pub, Attr *attrs) {
  Token t = advance(p); // consume 'type'
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected type name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_EQUAL, "expected '=' after type name");
  if (p->had_error)
    return NULL;

  // check for variant: type Color = | Red | Green ...
  if (check(p, TOKEN_PIPE)) {
    AstNode *arms = parse_variant_decl(p, is_pub);
    AstNode *vn =
        ast_new_variant_decl(p->arena, is_pub, name_tok.str_val.ptr, arms);
    vn->line = t.line;
    vn->col = t.column;
    return vn;
  }

  AstNode *fields = NULL, *ftail = NULL;
  bool braced = match(p, TOKEN_LBRACE);
  do {
    // optional field attributes: #[json("email_address")]
    Attr *attrs = parse_attrs(p);
    // optional volatile
    bool fvol = match(p, TOKEN_VOLATILE);
    Token fname = expect(p, TOKEN_IDENTIFIER, "expected field name");
    if (p->had_error)
      return NULL;
    expect(p, TOKEN_COLON, "expected ':' after field name");
    if (p->had_error)
      return NULL;
    AstNode *ftype = parse_type_expr(p);
    if (!ftype)
      return NULL;
    // optional default: = expr
    AstNode *fdefault = NULL;
    if (match(p, TOKEN_EQUAL)) {
      fdefault = parse_expr(p);
      if (!fdefault)
        return NULL;
    }
    AstNode *fd = ast_new_field_decl(p->arena, fvol, fname.str_val.ptr, ftype,
                                     fdefault, attrs);
    fd->line = fname.line;
    fd->col = fname.column;
    if (!fields) {
      fields = ftail = fd;
    } else {
      ftail->next = fd;
      ftail = fd;
    }
  } while (match(p, TOKEN_COMMA));

  if (braced) {
    expect(p, TOKEN_RBRACE, "expected '}' after type fields");
    if (p->had_error)
      return NULL;
  }

  // collect attrs that were on the type itself (parsed before this call)
  AstNode *n =
      ast_new_type_decl(p->arena, is_pub, name_tok.str_val.ptr, fields, attrs);
  n->line = t.line;
  n->col = t.column;
  return n;
}

// Spec §7: type Color = | Red | Green | RGB(u8,u8,u8)
static AstNode *parse_variant_decl(Parser *p, bool is_pub) {
  // 'type Name =' already consumed; current == TOKEN_PIPE
  // we need the name — reconstruct from the token that parse_type_decl consumed
  // Actually parse_type_decl calls us and we don't have name_tok.
  // Let's refactor: parse_variant_decl is called from parse_type_decl
  // with the parent context. But we lost name_tok...
  // For now: we'll set name to NULL and fix the caller.
  // Actually, let me restructure: parse_type_decl handles this inline.

  AstNode *arms = NULL, *atail = NULL;
  while (match(p, TOKEN_PIPE)) {
    Token vname = expect(p, TOKEN_IDENTIFIER, "expected variant name");
    if (p->had_error)
      return NULL;
    AstNode *vfields = NULL;
    if (match(p, TOKEN_LPAREN)) {
      AstNode *vfh = NULL, *vft = NULL;
      do {
        AstNode *vf = parse_type_expr(p);
        if (!vf)
          return NULL;
        if (!vfh) {
          vfh = vft = vf;
        } else {
          vft->next = vf;
          vft = vf;
        }
      } while (match(p, TOKEN_COMMA));
      expect(p, TOKEN_RPAREN, "expected ')' after variant fields");
      if (p->had_error)
        return NULL;
      vfields = vfh;
    }
    AstNode *arm = ast_new_variant_arm(p->arena, vname.str_val.ptr, vfields);
    arm->line = vname.line;
    arm->col = vname.column;
    if (!arms) {
      arms = atail = arm;
    } else {
      atail->next = arm;
      atail = arm;
    }
  }
  // name is passed from parse_type_decl via a trick — we'll return arms
  // and let parse_type_decl wrap it. This is a design compromise.
  return arms; // caller wraps in ast_new_variant_decl
}

// Spec §13: schema Shoe = { brand: str, size: f32 }
//           schema RedShoe : Shoe = { color: str = "red" }
static AstNode *parse_schema_decl(Parser *p, bool is_pub) {
  Token s = advance(p); // consume 'schema'
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected schema name");
  if (p->had_error)
    return NULL;

  // optional parent: schema RedShoe : Shoe
  const char *parent = NULL;
  if (match(p, TOKEN_COLON)) {
    Token parent_tok = expect(p, TOKEN_IDENTIFIER, "expected parent schema");
    if (p->had_error)
      return NULL;
    parent = parent_tok.str_val.ptr;
  }

  expect(p, TOKEN_EQUAL, "expected '=' after schema name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_LBRACE, "expected '{' for schema body");
  if (p->had_error)
    return NULL;

  AstNode *fields = NULL, *ftail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    Attr *attrs = parse_attrs(p);
    Token fname = expect(p, TOKEN_IDENTIFIER, "expected field name");
    if (p->had_error)
      return NULL;
    expect(p, TOKEN_COLON, "expected ':'");
    if (p->had_error)
      return NULL;
    AstNode *ftype = parse_type_expr(p);
    if (!ftype)
      return NULL;
    AstNode *fdef = NULL;
    if (match(p, TOKEN_EQUAL)) {
      fdef = parse_expr(p);
      if (!fdef)
        return NULL;
    }
    AstNode *fd = ast_new_field_decl(p->arena, false, fname.str_val.ptr, ftype,
                                     fdef, attrs);
    fd->line = fname.line;
    fd->col = fname.column;
    if (!fields) {
      fields = ftail = fd;
    } else {
      ftail->next = fd;
      ftail = fd;
    }
    match(p, TOKEN_COMMA);
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;

  AstNode *n = ast_new_schema_decl(p->arena, is_pub, name_tok.str_val.ptr,
                                   parent, fields);
  n->line = s.line;
  n->col = s.column;
  return n;
}

// Spec §7: method Vec2 { f length(self) = r: f32 { ... } }
//      or: method Drawable for Vec2 { ... }
static AstNode *parse_method_decl(Parser *p, bool is_pub) {
  Token m = advance(p); // consume 'method'
  Token type_tok = expect(p, TOKEN_IDENTIFIER, "expected type name");
  if (p->had_error)
    return NULL;

  const char *iface = NULL;
  if (match(p, TOKEN_FOR)) {
    // method Drawable for Vec2 — iface = "Drawable", type = "Vec2"
    iface = type_tok.str_val.ptr;
    type_tok = expect(p, TOKEN_IDENTIFIER, "expected type name after 'for'");
    if (p->had_error)
      return NULL;
  }

  expect(p, TOKEN_LBRACE, "expected '{' for method block");
  if (p->had_error)
    return NULL;

  AstNode *methods = NULL, *mtail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    Attr *attrs = parse_attrs(p);
    // each method is a function: [realm] f name(self, ...) = r: T { ... }
    MemoryRealm realm = REALM_STACK;
    if (match(p, TOKEN_REGIONAL))
      realm = REALM_ARENA;
    else if (match(p, TOKEN_DYNAMIC))
      realm = REALM_HEAP;
    else if (match(p, TOKEN_GC))
      realm = REALM_GC;
    else if (match(p, TOKEN_FLEX))
      realm = REALM_FLEX;

    AstNode *fn = parse_func_decl(p, false, realm, attrs);
    if (!fn) {
      synchronize(p);
      continue;
    }
    if (!methods) {
      methods = mtail = fn;
    } else {
      mtail->next = fn;
      mtail = fn;
    }
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;

  AstNode *n = ast_new_method_decl(p->arena, is_pub, type_tok.str_val.ptr,
                                   iface, methods);
  n->line = m.line;
  n->col = m.column;
  return n;
}

// Spec §7: interface Drawable { f draw(self) ... }
static AstNode *parse_interface_decl(Parser *p, bool is_pub) {
  Token i = advance(p); // consume 'interface'
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected interface name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_LBRACE, "expected '{'");
  if (p->had_error)
    return NULL;

  AstNode *methods = NULL, *mtail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    Attr *attrs = parse_attrs(p);
    AstNode *fn = parse_func_decl(p, false, REALM_STACK, attrs);
    if (!fn) {
      synchronize(p);
      continue;
    }
    if (!methods) {
      methods = mtail = fn;
    } else {
      mtail->next = fn;
      mtail = fn;
    }
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;

  AstNode *n =
      ast_new_interface_decl(p->arena, is_pub, name_tok.str_val.ptr, methods);
  n->line = i.line;
  n->col = i.column;
  return n;
}

// Spec §10: error MathError = { | DivByZero | Overflow }
static AstNode *parse_error_decl(Parser *p, bool is_pub) {
  Token e = advance(p); // consume 'error'
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected error set name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_EQUAL, "expected '='");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_LBRACE, "expected '{'");
  if (p->had_error)
    return NULL;

  AstNode *variants = NULL, *vtail = NULL;
  while (match(p, TOKEN_PIPE)) {
    Token vname = expect(p, TOKEN_IDENTIFIER, "expected error variant");
    if (p->had_error)
      return NULL;
    AstNode *v = ast_new_identifier(p->arena, vname.str_val.ptr);
    v->line = vname.line;
    v->col = vname.column;
    if (!variants) {
      variants = vtail = v;
    } else {
      vtail->next = v;
      vtail = v;
    }
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;

  AstNode *n =
      ast_new_error_decl(p->arena, is_pub, name_tok.str_val.ptr, variants);
  n->line = e.line;
  n->col = e.column;
  return n;
}

// Spec §14: mod kernel { pub f alloc_page() ... }
static AstNode *parse_mod_decl(Parser *p, bool is_pub) {
  Token m = advance(p);
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected module name");
  if (p->had_error)
    return NULL;
  expect(p, TOKEN_LBRACE, "expected '{'");
  if (p->had_error)
    return NULL;

  AstNode *decls = NULL, *dtail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    AstNode *d = parse_decl(p);
    if (!d) {
      synchronize(p);
      continue;
    }
    if (!decls) {
      decls = dtail = d;
    } else {
      dtail->next = d;
      dtail = d;
    }
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;

  AstNode *n = ast_new_mod_decl(p->arena, is_pub, name_tok.str_val.ptr, decls);
  n->line = m.line;
  n->col = m.column;
  return n;
}

// Spec §14: use kernel.arch.x86.read_cr3
static AstNode *parse_use_decl(Parser *p) {
  Token u = advance(p); // consume 'use'
  // path: ident.ident.ident...
  AstNode *head = NULL, *tail = NULL;
  do {
    Token seg = expect(p, TOKEN_IDENTIFIER, "expected module path");
    if (p->had_error)
      return NULL;
    AstNode *s = ast_new_identifier(p->arena, seg.str_val.ptr);
    s->line = seg.line;
    s->col = seg.column;
    if (!head) {
      head = tail = s;
    } else {
      tail->next = s;
      tail = s;
    }
  } while (match(p, TOKEN_DOT));

  AstNode *n = ast_new_use_decl(p->arena, head);
  n->line = u.line;
  n->col = u.column;
  return n;
}

// Spec §12: extern f memset(ptr: *u8, val: i32, len: usize)
//           extern u64 KERNEL_START
static AstNode *parse_extern_decl(Parser *p) {
  Token e = advance(p); // consume 'extern'

  if (check(p, TOKEN_F)) {
    // extern function
    advance(p); // consume 'f'
    Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected function name");
    if (p->had_error)
      return NULL;
    expect(p, TOKEN_LPAREN, "expected '('");
    if (p->had_error)
      return NULL;
    AstNode *params = parse_param_list(p);
    expect(p, TOKEN_RPAREN, "expected ')'");
    if (p->had_error)
      return NULL;
    // optional return: = name: type
    const char *ret_name = NULL;
    AstNode *ret_type = NULL;
    if (match(p, TOKEN_EQUAL)) {
      Token rn = expect(p, TOKEN_IDENTIFIER, "expected return name");
      if (p->had_error)
        return NULL;
      ret_name = rn.str_val.ptr;
      expect(p, TOKEN_COLON, "expected ':'");
      if (p->had_error)
        return NULL;
      ret_type = parse_type_expr(p);
      if (!ret_type)
        return NULL;
    }
    AstNode *n = ast_new_extern_decl(p->arena, true, name_tok.str_val.ptr,
                                     params, ret_name, ret_type, NULL);
    n->line = e.line;
    n->col = e.column;
    return n;
  } else {
    // extern variable: extern u64 KERNEL_START
    AstNode *vtype = parse_type_expr(p);
    if (!vtype)
      return NULL;
    Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected variable name");
    if (p->had_error)
      return NULL;
    AstNode *n = ast_new_extern_decl(p->arena, false, name_tok.str_val.ptr,
                                     NULL, NULL, NULL, vtype);
    n->line = e.line;
    n->col = e.column;
    return n;
  }
}

// Top-level declaration dispatcher
static AstNode *parse_decl(Parser *p) {
  Attr *attrs = parse_attrs(p);

  bool is_pub = match(p, TOKEN_PUB);

  // memory realm → function
  MemoryRealm realm = REALM_STACK;
  bool has_realm = false;
  if (match(p, TOKEN_STACK)) {
    realm = REALM_STACK;
    has_realm = true;
  } else if (match(p, TOKEN_REGIONAL)) {
    realm = REALM_ARENA;
    has_realm = true;
  } else if (match(p, TOKEN_DYNAMIC)) {
    realm = REALM_HEAP;
    has_realm = true;
  } else if (match(p, TOKEN_GC)) {
    realm = REALM_GC;
    has_realm = true;
  } else if (match(p, TOKEN_FLEX)) {
    realm = REALM_FLEX;
    has_realm = true;
  }

  if (check(p, TOKEN_F) || has_realm) {
    return parse_func_decl(p, is_pub, realm, attrs);
  }
  if (check(p, TOKEN_TYPE)) {
    return parse_type_decl(p, is_pub, attrs);
  }
  if (check(p, TOKEN_SCHEMA))
    return parse_schema_decl(p, is_pub);
  if (check(p, TOKEN_METHOD))
    return parse_method_decl(p, is_pub);
  if (check(p, TOKEN_INTERFACE))
    return parse_interface_decl(p, is_pub);
  if (check(p, TOKEN_ERROR))
    return parse_error_decl(p, is_pub);
  if (check(p, TOKEN_MOD))
    return parse_mod_decl(p, is_pub);
  if (check(p, TOKEN_USE))
    return parse_use_decl(p);
  if (check(p, TOKEN_EXTERN))
    return parse_extern_decl(p);

  parser_error(p, "expected a declaration");
  return NULL;
}

// public entry point
AstNode *parser_parse(Parser *p) {
  AstNode *head = NULL, *tail = NULL;
  while (!check(p, TOKEN_EOF)) {
    AstNode *d = parse_stmt(p);
    if (!d) {
      synchronize(p);
      continue;
    }
    if (!head) {
      head = tail = d;
    } else {
      tail->next = d;
      tail = d;
    }
  }
  return ast_new_program(p->arena, head);
}

static Attr *parse_attrs(Parser *p) {
  Attr *head = NULL, *tail = NULL;

  while (check(p, TOKEN_HASH)) {
    advance(p); // consume #
    expect(p, TOKEN_LBRACKET, "expected '[' after '#'");
    if (p->had_error)
      return NULL;

    Token name = expect(p, TOKEN_IDENTIFIER, "expected attribute name");
    if (p->had_error)
      return NULL;

    // optional argument — #[align(4096)] or #[section(".text.boot")]
    AstNode *arg = NULL;
    if (match(p, TOKEN_LPAREN)) {
      arg = parse_expr(p);
      if (!arg)
        return NULL;
      expect(p, TOKEN_RPAREN, "expected ')' after attribute argument");
      if (p->had_error)
        return NULL;
    }

    expect(p, TOKEN_RBRACKET, "expected ']' after attribute");
    if (p->had_error)
      return NULL;

    Attr *a = attr_new(p->arena, name.str_val.ptr, arg);
    if (!head) {
      head = tail = a;
    } else {
      tail->next = a;
      tail = a;
    }
  }

  return head; // NULL means no attributes — fine
}

// ── helpers ──────────────────────────────────────────────────────────────────
static bool is_type_keyword(TokenKind k) {
  return k == TOKEN_I8 || k == TOKEN_I16 || k == TOKEN_I32 || k == TOKEN_I64 ||
         k == TOKEN_U8 || k == TOKEN_U16 || k == TOKEN_U32 || k == TOKEN_U64 ||
         k == TOKEN_F32 || k == TOKEN_F64 || k == TOKEN_BOOL ||
         k == TOKEN_STR || k == TOKEN_CHAR || k == TOKEN_USIZE ||
         k == TOKEN_VOID;
}

// ── Statements ──────────────────────────────────────────────────────────────

// Spec §generic: { stmt* }
static AstNode *parse_block(Parser *p) {
  Token open = expect(p, TOKEN_LBRACE, "expected '{'");
  if (p->had_error)
    return NULL;
  AstNode *head = NULL, *tail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    AstNode *s = parse_stmt(p);
    if (!s) {
      synchronize(p);
      continue;
    }
    if (!head) {
      head = tail = s;
    } else {
      tail->next = s;
      tail = s;
    }
  }
  expect(p, TOKEN_RBRACE, "expected '}'");
  if (p->had_error)
    return NULL;
  AstNode *n = ast_new_block(p->arena, head);
  n->line = open.line;
  n->col = open.column;
  return n;
}

// Spec §6: if cond { ... } else if cond { ... } else { ... }
static AstNode *parse_if_stmt(Parser *p) {
  Token if_tok = advance(p); // consume 'if'
  AstNode *cond = parse_expr(p);
  if (!cond)
    return NULL;
  AstNode *then_b = parse_block(p);
  if (!then_b)
    return NULL;
  AstNode *else_b = NULL;
  if (match(p, TOKEN_ELSE)) {
    if (check(p, TOKEN_IF))
      else_b = parse_if_stmt(p);
    else
      else_b = parse_block(p);
    if (!else_b)
      return NULL;
  }
  AstNode *n = ast_new_if_stmt(p->arena, cond, then_b, else_b);
  n->line = if_tok.line;
  n->col = if_tok.column;
  return n;
}

// Spec §6: while cond { ... }
static AstNode *parse_while_stmt(Parser *p) {
  Token w = advance(p);
  AstNode *cond = parse_expr(p);
  if (!cond)
    return NULL;
  AstNode *body = parse_block(p);
  if (!body)
    return NULL;
  AstNode *n = ast_new_while_stmt(p->arena, cond, body);
  n->line = w.line;
  n->col = w.column;
  return n;
}

// Spec §6: for (iter) |val| { ... } / |*val| / |val, idx|
static AstNode *parse_for_stmt(Parser *p) {
  Token f = advance(p); // consume 'for'
  expect(p, TOKEN_LPAREN, "expected '(' after 'for'");
  if (p->had_error)
    return NULL;
  AstNode *iter = parse_expr(p);
  if (!iter)
    return NULL;
  expect(p, TOKEN_RPAREN, "expected ')' after for iterator");
  if (p->had_error)
    return NULL;

  // capture: |val| or |*val| or |val, idx|
  expect(p, TOKEN_PIPE, "expected '|' for capture");
  if (p->had_error)
    return NULL;

  CaptureKind cap_kind = CAPTURE_VALUE;
  const char *cap_value = NULL;
  const char *cap_index = NULL;

  if (match(p, TOKEN_STAR)) {
    cap_kind = CAPTURE_PTR; // |*n|
  }
  Token val_tok = expect(p, TOKEN_IDENTIFIER, "expected capture name");
  if (p->had_error)
    return NULL;
  cap_value = val_tok.str_val.ptr;

  if (match(p, TOKEN_COMMA)) {
    cap_kind = CAPTURE_INDEXED; // |n, i|
    Token idx_tok = expect(p, TOKEN_IDENTIFIER, "expected index name");
    if (p->had_error)
      return NULL;
    cap_index = idx_tok.str_val.ptr;
  }

  expect(p, TOKEN_PIPE, "expected '|' after capture");
  if (p->had_error)
    return NULL;

  AstNode *body = parse_block(p);
  if (!body)
    return NULL;
  AstNode *n =
      ast_new_for_stmt(p->arena, iter, cap_kind, cap_value, cap_index, body);
  n->line = f.line;
  n->col = f.column;
  return n;
}

// Spec §6: loop { ... }
static AstNode *parse_loop_stmt(Parser *p) {
  Token l = advance(p);
  AstNode *body = parse_block(p);
  if (!body)
    return NULL;
  AstNode *n = ast_new_loop_stmt(p->arena, body);
  n->line = l.line;
  n->col = l.column;
  return n;
}

// Spec §9: match subject { pattern -> body, ... }
static AstNode *parse_match_arm(Parser *p) {
  // pattern — for now: expression (covers literals, identifiers, destructuring)
  AstNode *pattern = parse_expr(p);
  if (!pattern)
    return NULL;

  // optional guard: if cond
  AstNode *guard = NULL;
  if (match(p, TOKEN_IF)) {
    guard = parse_expr(p);
    if (!guard)
      return NULL;
  }

  expect(p, TOKEN_ARROW, "expected '->' after match pattern");
  if (p->had_error)
    return NULL;

  AstNode *body = parse_expr(p);
  if (!body)
    return NULL;

  AstNode *n = ast_new_match_arm(p->arena, pattern, guard, body);
  n->line = pattern->line;
  n->col = pattern->col;
  return n;
}

static AstNode *parse_match_stmt(Parser *p) {
  Token m = advance(p); // consume 'match'
  AstNode *subject = parse_expr(p);
  if (!subject)
    return NULL;
  expect(p, TOKEN_LBRACE, "expected '{' after match subject");
  if (p->had_error)
    return NULL;

  AstNode *head = NULL, *tail = NULL;
  while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
    AstNode *arm = parse_match_arm(p);
    if (!arm) {
      synchronize(p);
      continue;
    }
    if (!head) {
      head = tail = arm;
    } else {
      tail->next = arm;
      tail = arm;
    }
    match(p, TOKEN_COMMA); // optional trailing comma
  }
  expect(p, TOKEN_RBRACE, "expected '}' after match arms");
  if (p->had_error)
    return NULL;

  AstNode *n = ast_new_match_stmt(p->arena, subject, head);
  n->line = m.line;
  n->col = m.column;
  return n;
}

// Spec §11: unsafe { ... }
static AstNode *parse_unsafe_block(Parser *p) {
  Token u = advance(p);
  AstNode *body = parse_block(p);
  if (!body)
    return NULL;
  AstNode *n = ast_new_unsafe_block(p->arena, body);
  n->line = u.line;
  n->col = u.column;
  return n;
}

// Top-level statement dispatcher
static AstNode *parse_stmt(Parser *p) {
  // Spec §6: return expr
  if (check(p, TOKEN_RETURN)) {
    Token r = advance(p);
    AstNode *val = NULL;
    if (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF))
      val = parse_expr(p);
    AstNode *n = ast_new_return_stmt(p->arena, val);
    n->line = r.line;
    n->col = r.column;
    return n;
  }
  if (check(p, TOKEN_BREAK)) {
    Token b = advance(p);
    AstNode *n = ast_new_break(p->arena);
    n->line = b.line;
    n->col = b.column;
    return n;
  }
  if (check(p, TOKEN_CONTINUE)) {
    Token c = advance(p);
    AstNode *n = ast_new_continue(p->arena);
    n->line = c.line;
    n->col = c.column;
    return n;
  }
  // control-flow statements
  if (check(p, TOKEN_IF))
    return parse_if_stmt(p);
  if (check(p, TOKEN_WHILE))
    return parse_while_stmt(p);
  if (check(p, TOKEN_FOR))
    return parse_for_stmt(p);
  if (check(p, TOKEN_LOOP))
    return parse_loop_stmt(p);
  if (check(p, TOKEN_MATCH))
    return parse_match_stmt(p);
  if (check(p, TOKEN_UNSAFE))
    return parse_unsafe_block(p);
  // declarations or expression statements
  if (check(p, TOKEN_PUB) || check(p, TOKEN_F) || check(p, TOKEN_TYPE) ||
      check(p, TOKEN_SCHEMA) || check(p, TOKEN_ERROR) || check(p, TOKEN_MOD) ||
      check(p, TOKEN_USE) || check(p, TOKEN_EXTERN) || check(p, TOKEN_HASH) ||
      check(p, TOKEN_REGIONAL) || check(p, TOKEN_DYNAMIC) ||
      check(p, TOKEN_GC) || check(p, TOKEN_FLEX) || check(p, TOKEN_STACK) ||
      check(p, TOKEN_METHOD) || check(p, TOKEN_INTERFACE))
    return parse_decl(p);

  // Variable declarations routing
  bool is_decl = false;
  if (check(p, TOKEN_CONST) || check(p, TOKEN_VOLATILE) ||
      is_type_keyword(p->current.kind) ||
      check(p, TOKEN_STAR) || check(p, TOKEN_LBRACKET) ||
      check(p, TOKEN_BANG) || check(p, TOKEN_LPAREN)) {
    is_decl = true;
  } else if (check(p, TOKEN_IDENTIFIER)) {
    // Lookahead for IDENT IDENT pattern
    if (peek(p).kind == TOKEN_IDENTIFIER) {
      is_decl = true;
    }
  }

  if (is_decl) {
    bool is_const = match(p, TOKEN_CONST);
    bool is_volatile = match(p, TOKEN_VOLATILE);
    return parse_var_decl(p, is_const, is_volatile);
  }

  return parse_expr(p);
}

// ── Expressions
// ───────────────────────────────────────────────────────────────
static AstNode *parse_assign(Parser *p) {
  AstNode *left = parse_catch(p); // parse left side first
  if (!left)
    return NULL;

  if (check(p, TOKEN_EQUAL)) {
    Token eq = advance(p);
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
      handler = parse_try(p);
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
  if (check(p, TOKEN_SELF)) {
    Token self_tok = advance(p);
    // self has implicit type in methods.
    // We pass NULL for type; typechecker will fill it.
    AstNode *n = ast_new_param(p->arena, "self", NULL);
    n->line = self_tok.line;
    n->col = self_tok.column;
    return n;
  }

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
  // ── literals ──────────────────────────────────────────────────────────
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

  // ── self keyword — Spec §7: method Vec2 { f length(self) ... } ──────
  if (check(p, TOKEN_SELF)) {
    Token tok = advance(p);
    AstNode *n = ast_new_identifier(p->arena, "self");
    n->line = tok.line;
    n->col = tok.column;
    return n;
  }

  // ── promote(expr) as realm — Spec §5: promote(&t) as dynamic ────────
  if (check(p, TOKEN_PROMOTE)) {
    Token prom_tok = advance(p);
    expect(p, TOKEN_LPAREN, "expected '(' after 'promote'");
    if (p->had_error)
      return NULL;
    AstNode *expr = parse_expr(p);
    if (!expr)
      return NULL;
    expect(p, TOKEN_RPAREN, "expected ')' after promote expression");
    if (p->had_error)
      return NULL;
    expect(p, TOKEN_AS, "expected 'as' after promote()");
    if (p->had_error)
      return NULL;
    // target realm keyword
    MemoryRealm target;
    if (check(p, TOKEN_DYNAMIC)) {
      advance(p);
      target = REALM_HEAP;
    } else if (check(p, TOKEN_GC)) {
      advance(p);
      target = REALM_GC;
    } else {
      parser_error(p, "promote target must be 'dynamic' or 'gc'");
      return NULL;
    }
    AstNode *n = ast_new_promote(p->arena, expr, target);
    n->line = prom_tok.line;
    n->col = prom_tok.column;
    return n;
  }

  // ── error.MathError.DivByZero — Spec §10 ────────────────────────────
  if (check(p, TOKEN_ERROR)) {
    Token err_tok = advance(p);
    AstNode *head = NULL, *tail = NULL;
    while (match(p, TOKEN_DOT)) {
      Token seg = expect(p, TOKEN_IDENTIFIER, "expected error name after '.'");
      if (p->had_error)
        return NULL;
      AstNode *seg_node = ast_new_identifier(p->arena, seg.str_val.ptr);
      seg_node->line = seg.line;
      seg_node->col = seg.column;
      if (!head) {
        head = tail = seg_node;
      } else {
        tail->next = seg_node;
        tail = seg_node;
      }
    }
    AstNode *n = ast_new_error_expr(p->arena, head);
    n->line = err_tok.line;
    n->col = err_tok.column;
    return n;
  }

  // ── if-as-expression — Spec §6: str label = if x > 0 { "pos" } else { "neg"
  // }
  if (check(p, TOKEN_IF)) {
    Token if_tok = advance(p);
    AstNode *condition = parse_expr(p);
    if (!condition)
      return NULL;
    AstNode *then_branch = parse_block(p);
    if (!then_branch)
      return NULL;
    AstNode *else_branch = NULL;
    if (match(p, TOKEN_ELSE)) {
      if (check(p, TOKEN_IF)) {
        // else if — recurse
        else_branch = parse_primary(p); // re-enter at if
      } else {
        else_branch = parse_block(p);
      }
      if (!else_branch)
        return NULL;
    }
    AstNode *n = ast_new_if_stmt(p->arena, condition, then_branch, else_branch);
    n->line = if_tok.line;
    n->col = if_tok.column;
    return n;
  }

  // ── array literal — Spec §3: [1, 2, 3] or [] (zero-initialized) ─────
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

  // ── parenthesized expression or tuple — Spec §5: (a, b) ────────────
  if (check(p, TOKEN_LPAREN)) {
    Token tok = advance(p);
    AstNode *first = parse_expr(p);
    if (!first)
      return NULL;
    if (match(p, TOKEN_COMMA)) {
      // tuple: (a, b, ...)
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

  // ── identifier ──────────────────────────────────────────────────────
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
    // ── call: foo(a, b) — Spec §4 ─────────────────────────────────────
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

      // ── index: arr[0] — Spec §3 ───────────────────────────────────────
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

      // ── field: foo.bar — Spec §7 ──────────────────────────────────────
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

      // ── range: 0..10 or 0..=10 — Spec §6 ─────────────────────────────
    } else if (check(p, TOKEN_RANGE)) {
      Token range_tok = advance(p);
      AstNode *end = parse_additive(p); // range binds tighter than comparison
      if (!end)
        return NULL;
      AstNode *n = ast_new_range_expr(p->arena, left, end, false);
      n->line = range_tok.line;
      n->col = range_tok.column;
      left = n;

    } else if (check(p, TOKEN_RANGE_INC)) {
      Token range_tok = advance(p);
      AstNode *end = parse_additive(p);
      if (!end)
        return NULL;
      AstNode *n = ast_new_range_expr(p->arena, left, end, true);
      n->line = range_tok.line;
      n->col = range_tok.column;
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
    AstNode *arg = NULL;
    // Named argument: x: 1.0
    if (check(p, TOKEN_IDENTIFIER) && peek(p).kind == TOKEN_COLON) {
      Token name = advance(p);
      advance(p); // consume ':'
      AstNode *value = parse_expr(p);
      if (!value)
        return NULL;
      arg = ast_new_named_arg(p->arena, name.str_val.ptr, value);
      arg->line = name.line;
      arg->col = name.column;
    } else {
      arg = parse_expr(p);
    }

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