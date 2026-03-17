#include "ast.h"
#include <stdio.h>
#include <string.h>

static void indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

static const char* realm_to_string(MemoryRealm realm) {
    switch (realm) {
        case REALM_STACK: return "stack";
        case REALM_ARENA: return "regional";
        case REALM_HEAP: return "dynamic";
        case REALM_GC: return "gc";
        case REALM_FLEX: return "flex";
        case REALM_MAIN: return "main";
        default: return "unknown";
    }
}

static const char* type_kind_to_string(TypeKind kind) {
    switch (kind) {
        case TYPE_NAMED: return "named";
        case TYPE_PTR: return "pointer";
        case TYPE_ARRAY: return "array";
        case TYPE_SL: return "sl";
        case TYPE_DL: return "dl";
        case TYPE_FALLIBLE: return "fallible";
        case TYPE_TUPLE: return "tuple";
        case TYPE_J: return "J";
        default: return "unknown";
    }
}

void ast_print_ext(AstNode *node, int level) {
    if (!node) return;

    indent(level);

    switch (node->kind) {
        case AST_PROGRAM:
            printf("Program\n");
            ast_print_ext(node->as.program.declarations, level + 1);
            break;

        case AST_FUNC_DECL:
            printf("FuncDecl name='%s' realm=%s is_pub=%s is_main=%s\n",
                   node->as.func_decl.name,
                   realm_to_string(node->as.func_decl.realm),
                   node->as.func_decl.is_pub ? "true" : "false",
                   node->as.func_decl.is_main ? "true" : "false");
            if (node->as.func_decl.params) {
                indent(level + 1); printf("Params:\n");
                ast_print_ext(node->as.func_decl.params, level + 2);
            }
            if (node->as.func_decl.ret_name) {
                indent(level + 1); printf("Return name='%s'\n", node->as.func_decl.ret_name);
                ast_print_ext(node->as.func_decl.ret_type, level + 2);
            }
            if (node->as.func_decl.body) {
                indent(level + 1); printf("Body:\n");
                ast_print_ext(node->as.func_decl.body, level + 2);
            }
            break;

        case AST_PARAM:
            printf("Param name='%s'\n", node->as.param.name);
            ast_print_ext(node->as.param.type, level + 1);
            break;

        case AST_VAR_DECL:
            printf("VarDecl name='%s' is_const=%s is_volatile=%s\n",
                   node->as.var_decl.name,
                   node->as.var_decl.is_const ? "true" : "false",
                   node->as.var_decl.is_volatile ? "true" : "false");
            if (node->as.var_decl.type) {
                ast_print_ext(node->as.var_decl.type, level + 1);
            }
            if (node->as.var_decl.init) {
                indent(level + 1); printf("Init:\n");
                ast_print_ext(node->as.var_decl.init, level + 2);
            }
            break;

        case AST_TYPE_DECL:
            printf("TypeDecl name='%s' is_pub=%s\n",
                   node->as.type_decl.name,
                   node->as.type_decl.is_pub ? "true" : "false");
            ast_print_ext(node->as.type_decl.fields, level + 1);
            break;

        case AST_FIELD_DECL:
            printf("FieldDecl name='%s' is_volatile=%s\n",
                   node->as.field_decl.name,
                   node->as.field_decl.is_volatile ? "true" : "false");
            ast_print_ext(node->as.field_decl.type, level + 1);
            if (node->as.field_decl.default_val) {
                indent(level + 1); printf("Default:\n");
                ast_print_ext(node->as.field_decl.default_val, level + 2);
            }
            break;

        case AST_BLOCK:
            printf("Block\n");
            ast_print_ext(node->as.block.statements, level + 1);
            break;

        case AST_BINARY_EXPR:
            printf("BinaryExpr op=%s\n", token_kind_to_string(node->as.binary.op));
            ast_print_ext(node->as.binary.left, level + 1);
            ast_print_ext(node->as.binary.right, level + 1);
            break;

        case AST_UNARY_EXPR:
            printf("UnaryExpr op=%s\n", token_kind_to_string(node->as.unary.op));
            ast_print_ext(node->as.unary.expr, level + 1);
            break;

        case AST_IDENTIFIER:
            printf("Identifier name='%s'\n", node->as.identifier.name);
            break;

        case AST_INT_LITERAL:
            printf("IntLiteral value=%lld\n", node->as.int_literal.value);
            break;

        case AST_FLOAT_LITERAL:
            printf("FloatLiteral value=%f\n", node->as.float_literal.value);
            break;

        case AST_STRING_LITERAL:
            printf("StringLiteral value=\"%s\"\n", node->as.string_literal.value);
            break;

        case AST_BOOL_LITERAL:
            printf("BoolLiteral value=%s\n", node->as.bool_literal.value ? "true" : "false");
            break;

        case AST_CHAR_LITERAL:
            printf("CharLiteral codepoint=U+%04X\n", node->as.char_literal.codepoint);
            break;

        case AST_TYPE_EXPR:
            printf("TypeExpr kind=%s", type_kind_to_string(node->as.type_expr.kind));
            if (node->as.type_expr.kind == TYPE_NAMED) {
                printf(" name='%s'", node->as.type_expr.name);
            }
            printf("\n");
            if (node->as.type_expr.inner) {
                ast_print_ext(node->as.type_expr.inner, level + 1);
            }
            if (node->as.type_expr.size) {
                indent(level + 1); printf("Size:\n");
                ast_print_ext(node->as.type_expr.size, level + 2);
            }
            if (node->as.type_expr.elems) {
                ast_print_ext(node->as.type_expr.elems, level + 1);
            }
            break;

        case AST_CALL_EXPR:
            printf("CallExpr\n");
            indent(level + 1); printf("Callee:\n");
            ast_print_ext(node->as.call.callee, level + 2);
            if (node->as.call.args) {
                indent(level + 1); printf("Args:\n");
                ast_print_ext(node->as.call.args, level + 2);
            }
            break;

        case AST_IF_STMT:
            printf("IfStmt\n");
            indent(level + 1); printf("Condition:\n");
            ast_print_ext(node->as.if_stmt.condition, level + 2);
            indent(level + 1); printf("Then:\n");
            ast_print_ext(node->as.if_stmt.then_branch, level + 2);
            if (node->as.if_stmt.else_branch) {
                indent(level + 1); printf("Else:\n");
                ast_print_ext(node->as.if_stmt.else_branch, level + 2);
            }
            break;

        case AST_RETURN_STMT:
            printf("ReturnStmt\n");
            if (node->as.return_stmt.value) {
                ast_print_ext(node->as.return_stmt.value, level + 1);
            }
            break;

        case AST_ASSIGN:
            printf("Assign\n");
            indent(level + 1); printf("Target:\n");
            ast_print_ext(node->as.assign.target, level + 2);
            indent(level + 1); printf("Value:\n");
            ast_print_ext(node->as.assign.value, level + 2);
            break;

        default:
            printf("Node kind=%d (printing not fully implemented)\n", node->kind);
            break;
    }

    if (node->next) {
        ast_print_ext(node->next, level);
    }
}

void ast_print(AstNode *node) {
    ast_print_ext(node, 0);
}
