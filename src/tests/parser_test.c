#include "../parser.h"
#include "../ast.h"
#include "../lexer.h"
#include "../utils/arena.h"
#include "../utils/strtab.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_attributes() {
    printf("Running test_attributes...\n");
    Arena arena;
    assert(arena_init(&arena));

    const char *source = 
        "#[align(4096)]\n"
        "type PageTable = { entries: [512]u64 }\n"
        "\n"
        "#[interrupt]\n"
        "f handle_irq() { }\n";

    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    
    Parser parser;
    parser_init(&parser, &lexer, &arena, "test.runes", source);
    
    AstNode *program = parser_parse(&parser);
    assert(program != NULL);
    assert(program->kind == AST_PROGRAM);
    
    AstNode *decls = program->as.program.declarations;
    assert(decls != NULL);
    
    // Check PageTable attributes
    assert(decls->kind == AST_TYPE_DECL);
    assert(strcmp(decls->as.type_decl.name, "PageTable") == 0);
    assert(decls->as.type_decl.attrs != NULL);
    assert(strcmp(decls->as.type_decl.attrs->name, "align") == 0);
    assert(decls->as.type_decl.attrs->arg != NULL);
    assert(decls->as.type_decl.attrs->arg->kind == AST_INT_LITERAL);
    assert(decls->as.type_decl.attrs->arg->as.int_literal.value == 4096);
    
    // Check handle_irq attributes
    AstNode *fn_decl = decls->next;
    assert(fn_decl != NULL);
    assert(fn_decl->kind == AST_FUNC_DECL);
    assert(strcmp(fn_decl->as.func_decl.name, "handle_irq") == 0);
    assert(fn_decl->as.func_decl.attrs != NULL);
    assert(strcmp(fn_decl->as.func_decl.attrs->name, "interrupt") == 0);
    assert(fn_decl->as.func_decl.attrs->arg == NULL);
    
    printf("test_attributes passed!\n");
    arena_destroy(&arena);
}

void test_bare_return_stops_at_newline() {
    printf("Running test_bare_return_stops_at_newline...\n");
    Arena arena;
    assert(arena_init(&arena));

    const char *source =
        "f main() {\n"
        "  return\n"
        "  print(\"still a separate statement\")\n"
        "}\n";

    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "return_test.runes", source);

    AstNode *program = parser_parse(&parser);
    assert(program != NULL);
    assert(!parser.had_error);
    AstNode *main_fn = program->as.program.declarations;
    assert(main_fn != NULL && main_fn->kind == AST_FUNC_DECL);
    AstNode *return_stmt = main_fn->as.func_decl.body->as.block.statements;
    assert(return_stmt != NULL && return_stmt->kind == AST_RETURN_STMT);
    assert(return_stmt->as.return_stmt.value == NULL);
    assert(return_stmt->next != NULL && return_stmt->next->kind == AST_CALL_EXPR);

    printf("test_bare_return_stops_at_newline passed!\n");
    arena_destroy(&arena);
}

void test_generic_syntax() {
    printf("Running test_generic_syntax...\n");
    Arena arena;
    assert(arena_init(&arena));

    const char *source =
        "interface Value { f value(self) = result: i32 }\n"
        "type Pair<T, U: Value> = { first: T, second: U }\n"
        "type Maybe<T> = | None | Some(T)\n"
        "f identity<T>(value: T) = result: T { result = value }\n"
        "method Pair<T, U> { f keep<V>(self, value: V) = result: V { result = value } }\n"
        "f main() {\n"
        "  Pair<i32, Maybe<i32>> pair\n"
        "  i32 answer = identity<i32>(42)\n"
        "  bool ordered = 1 < 2\n"
        "}\n";

    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "generic_test.runes", source);
    AstNode *program = parser_parse(&parser);
    assert(program != NULL && !parser.had_error);

    AstNode *pair = program->as.program.declarations->next;
    assert(pair && pair->kind == AST_TYPE_DECL);
    AstNode *t = pair->as.type_decl.generic_params;
    assert(t && strcmp(t->as.param.name, "T") == 0 && !t->as.param.type);
    assert(t->next && strcmp(t->next->as.param.name, "U") == 0);
    assert(t->next->as.param.type &&
           strcmp(t->next->as.param.type->as.type_expr.name, "Value") == 0);

    AstNode *maybe = pair->next;
    AstNode *identity = maybe->next;
    AstNode *methods = identity->next;
    AstNode *main_fn = methods->next;
    assert(maybe->kind == AST_VARIANT_DECL &&
           maybe->as.variant_decl.generic_params);
    assert(identity->kind == AST_FUNC_DECL &&
           identity->as.func_decl.generic_params);
    assert(methods->kind == AST_METHOD_DECL && methods->as.method_decl.type_args);
    assert(methods->as.method_decl.methods->as.func_decl.generic_params);

    AstNode *statement = main_fn->as.func_decl.body->as.block.statements;
    assert(statement->kind == AST_VAR_DECL);
    assert(statement->as.var_decl.type->as.type_expr.type_args);
    AstNode *answer = statement->next;
    assert(answer->as.var_decl.init->kind == AST_CALL_EXPR);
    assert(answer->as.var_decl.init->as.call.type_args);
    AstNode *ordered = answer->next;
    assert(ordered->as.var_decl.init->kind == AST_BINARY_EXPR);
    assert(ordered->as.var_decl.init->as.binary.op == TOKEN_LT);

    printf("test_generic_syntax passed!\n");
    arena_destroy(&arena);
}

void test_import_alias_syntax() {
    printf("Running test_import_alias_syntax...\n");
    Arena arena;
    assert(arena_init(&arena));

    const char *source =
        "use std.core.Option as Maybe\n"
        "use std.bytes.find\n";

    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "import_alias_test.runes", source);
    AstNode *program = parser_parse(&parser);
    assert(program != NULL && !parser.had_error);

    AstNode *aliased = program->as.program.declarations;
    assert(aliased && aliased->kind == AST_USE_DECL);
    assert(aliased->as.use_decl.alias &&
           strcmp(aliased->as.use_decl.alias, "Maybe") == 0);
    assert(strcmp(aliased->as.use_decl.path->as.identifier.name, "std") == 0);
    assert(strcmp(aliased->as.use_decl.path->next->as.identifier.name,
                  "core") == 0);
    assert(strcmp(aliased->as.use_decl.path->next->next->as.identifier.name,
                  "Option") == 0);

    AstNode *plain = aliased->next;
    assert(plain && plain->kind == AST_USE_DECL);
    assert(plain->as.use_decl.alias == NULL);

    printf("test_import_alias_syntax passed!\n");
    arena_destroy(&arena);
}

void test_realm_block_syntax() {
    printf("Running test_realm_block_syntax...\n");
    Arena arena;
    assert(arena_init(&arena));
    const char *source =
        "flex f choose() = result: i32 {\n"
        "  when realm regional { result = 3 } else { result = 4 }\n"
        "}\n";
    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "realm_block_test.runes", source);
    AstNode *program = parser_parse(&parser);
    assert(program != NULL && !parser.had_error);
    AstNode *function = program->as.program.declarations;
    AstNode *block = function->as.func_decl.body->as.block.statements;
    assert(block && block->kind == AST_REALM_BLOCK);
    assert(block->as.realm_block.realm == EFFECTIVE_REALM_REGIONAL);
    assert(block->as.realm_block.body &&
           block->as.realm_block.body->kind == AST_BLOCK);
    assert(block->as.realm_block.else_branch &&
           block->as.realm_block.else_branch->kind == AST_BLOCK);
    arena_destroy(&arena);
    printf("test_realm_block_syntax passed!\n");
}

static void discard_parser_diagnostic(void *context, const char *filename,
                                      uint32_t line, uint32_t column,
                                      const char *message) {
    (void)context;
    (void)filename;
    (void)line;
    (void)column;
    (void)message;
}

static bool parse_has_error(const char *source) {
    Arena arena;
    assert(arena_init(&arena));
    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "realm_family_error.runes", source);
    parser_set_diagnostic_handler(&parser, discard_parser_diagnostic, NULL);
    AstNode *program = parser_parse(&parser);
    bool failed = !program || parser.had_error;
    arena_destroy(&arena);
    return failed;
}

void test_realm_overload_family_syntax() {
    printf("Running test_realm_overload_family_syntax...\n");
    Arena arena;
    assert(arena_init(&arena));
    const char *source =
        "#[inline]\n"
        "pub except(stack) flex f select<T>(value: T) = result: T {\n"
        "  result = value\n"
        "}\n"
        "#[inline]\n"
        "pub in dynamic except(stack) f select<T>(value: T) = result: T {\n"
        "  result = value\n"
        "}\n"
        "in dynamic type Cache<T> = { value: T }\n"
        "in gc type Cache<T> = { value: T }\n"
        "in dynamic interface Ready { f ready(self) = result: bool }\n"
        "in gc interface Ready { f ready(self) = result: bool }\n"
        "method Cache<T> {\n"
        "  flex f len(self) = result: usize { result = 1 }\n"
        "  in gc f len(self) = result: usize { result = 2 }\n"
        "}\n"
        "in dynamic method Box { f clear(self) {} }\n"
        "in gc method Box { f clear(self) {} }\n";
    StrTab strtab;
    strtab_init(&strtab, &arena);
    Lexer lexer;
    lexer_init(&lexer, source, &strtab);
    Parser parser;
    parser_init(&parser, &lexer, &arena, "realm_family_test.runes", source);
    AstNode *program = parser_parse(&parser);
    assert(program != NULL && !parser.had_error);

    AstNode *fallback = program->as.program.declarations;
    AstNode *dynamic = fallback->next;
    assert(!fallback->has_overload_realm);
    assert(fallback->as.func_decl.has_declared_realm);
    assert(fallback->as.func_decl.is_pub);
    assert(fallback->decl_attrs != NULL);
    assert(dynamic->has_overload_realm);
    assert(!dynamic->as.func_decl.has_declared_realm);
    assert(dynamic->overload_realm == EFFECTIVE_REALM_DYNAMIC);
    assert(fallback->excluded_realms ==
           (1u << EFFECTIVE_REALM_STACK));
    assert(fallback->realm_family_root == fallback);
    assert(dynamic->realm_family_root == fallback);
    assert(fallback->realm_family_fallback == fallback);
    assert(fallback->realm_family_variants[EFFECTIVE_REALM_DYNAMIC] ==
           dynamic);

    AstNode *cache_dynamic = dynamic->next;
    AstNode *cache_gc = cache_dynamic->next;
    assert(cache_dynamic->realm_family_root == cache_dynamic);
    assert(cache_dynamic->realm_family_fallback == NULL);
    assert(cache_dynamic->realm_family_variants[EFFECTIVE_REALM_GC] ==
           cache_gc);

    AstNode *methods = cache_gc->next->next->next;
    assert(methods && methods->kind == AST_METHOD_DECL);
    AstNode *len_fallback = methods->as.method_decl.methods;
    AstNode *len_gc = len_fallback->next;
    assert(len_fallback->realm_family_root == len_fallback);
    assert(len_gc->realm_family_root == len_fallback);
    assert(len_fallback->realm_family_variants[EFFECTIVE_REALM_GC] == len_gc);
    AstNode *box_dynamic = methods->next;
    AstNode *box_gc = box_dynamic->next;
    assert(box_dynamic->kind == AST_METHOD_DECL);
    assert(box_dynamic->realm_family_root == box_dynamic);
    assert(box_dynamic->realm_family_variants[EFFECTIVE_REALM_GC] == box_gc);

    arena_destroy(&arena);
    printf("test_realm_overload_family_syntax passed!\n");
}

void test_invalid_realm_overload_families() {
    printf("Running test_invalid_realm_overload_families...\n");
    assert(parse_has_error(
        "in gc f choose() {}\n"
        "in gc f choose() {}\n"));
    assert(parse_has_error(
        "f choose() {}\n"
        "f choose() {}\n"
        "in gc f choose() {}\n"));
    assert(parse_has_error(
        "pub f choose() {}\n"
        "in gc f choose() {}\n"));
    assert(parse_has_error(
        "f choose<T>() {}\n"
        "in gc f choose<T, U>() {}\n"));
    assert(parse_has_error("except(gc) in gc f choose() {}\n"));
    assert(parse_has_error("except() f choose() {}\n"));
    assert(parse_has_error("except(stack stack) f choose() {}\n"));
    assert(parse_has_error("except(stack, stack) f choose() {}\n"));
    assert(parse_has_error("except(stack, flex) f choose() {}\n"));
    assert(parse_has_error("except(stack f choose() {}\n"));
    assert(parse_has_error("in flex f choose() {}\n"));
    assert(parse_has_error("in gc error Nope = { | Bad }\n"));
    assert(parse_has_error("in gc const i32 VALUE = 1\n"));
    assert(parse_has_error(
        "f outer() {\n"
        "  in gc f nested() {}\n"
        "}\n"));
    printf("test_invalid_realm_overload_families passed!\n");
}

int main() {
    test_attributes();
    test_bare_return_stops_at_newline();
    test_generic_syntax();
    test_import_alias_syntax();
    test_realm_block_syntax();
    test_realm_overload_family_syntax();
    test_invalid_realm_overload_families();
    return 0;
}
