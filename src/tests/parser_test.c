#include "../parser.h"
#include "../ast.h"
#include "../lexer.h"
#include "../utils/arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_attributes() {
    printf("Running test_attributes...\n");
    Arena arena;
    arena_init(&arena, 1024 * 1024);

    const char *source = 
        "#[align(4096)]\n"
        "type PageTable = { entries: [512]u64 }\n"
        "\n"
        "#[interrupt]\n"
        "f handle_irq() { }\n";

    Lexer lexer;
    lexer_init(&lexer, source, "test.runes");
    
    Parser parser;
    parser_init(&parser, &lexer, &arena, "test.runes", source);
    
    AstNode *program = parse_program(&parser);
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
    arena_free(&arena);
}

int main() {
    test_attributes();
    return 0;
}
