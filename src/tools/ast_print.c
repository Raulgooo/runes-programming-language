#include "ast.h"
#include <stdio.h>
#include <string.h>

static void indent(int level) {
  for (int i = 0; i < level; i++)
    printf("  ");
}

static const char *realm_to_string(MemoryRealm realm) {
  switch (realm) {
  case REALM_STACK:
    return "stack";
  case REALM_ARENA:
    return "regional";
  case REALM_HEAP:
    return "dynamic";
  case REALM_GC:
    return "gc";
  case REALM_FLEX:
    return "flex";
  case REALM_MAIN:
    return "main";
  default:
    return "unknown";
  }
}

static const char *type_kind_to_string(TypeKind kind) {
  switch (kind) {
  case TYPE_NAMED:
    return "named";
  case TYPE_QUALIFIED:
    return "qualified";
  case TYPE_PTR:
    return "pointer";
  case TYPE_ARRAY:
    return "array";
  case TYPE_SL:
    return "sl";
  case TYPE_DL:
    return "dl";
  case TYPE_FALLIBLE:
    return "fallible";
  case TYPE_TUPLE:
    return "tuple";
  case TYPE_J:
    return "J";
  default:
    return "unknown";
  }
}

void ast_print_ext(AstNode *node, int level) {
  if (!node)
    return;

  indent(level);

  switch (node->kind) {
  case AST_PROGRAM:
    printf("Program\n");
    ast_print_ext(node->as.program.declarations, level + 1);
    break;

  case AST_FUNC_DECL:
    printf("FuncDecl name='%s' realm=%s is_pub=%s is_main=%s\n",
           node->as.func_decl.name ? node->as.func_decl.name : "(null)",
           realm_to_string(node->as.func_decl.realm),
           node->as.func_decl.is_pub ? "true" : "false",
           node->as.func_decl.is_main ? "true" : "false");
    if (node->as.func_decl.params) {
      indent(level + 1);
      printf("Params:\n");
      ast_print_ext(node->as.func_decl.params, level + 2);
    }
    if (node->as.func_decl.ret_name) {
      indent(level + 1);
      printf("Return name='%s'\n", node->as.func_decl.ret_name);
      ast_print_ext(node->as.func_decl.ret_type, level + 2);
    }
    if (node->as.func_decl.body) {
      indent(level + 1);
      printf("Body:\n");
      ast_print_ext(node->as.func_decl.body, level + 2);
    }
    break;

  case AST_PARAM:
    printf("Param name='%s'\n",
           node->as.param.name ? node->as.param.name : "(null)");
    ast_print_ext(node->as.param.type, level + 1);
    break;

  case AST_VAR_DECL:
    printf("VarDecl name='%s' is_const=%s is_volatile=%s\n",
           node->as.var_decl.name ? node->as.var_decl.name : "(null)",
           node->as.var_decl.is_const ? "true" : "false",
           node->as.var_decl.is_volatile ? "true" : "false");
    if (node->as.var_decl.type) {
      ast_print_ext(node->as.var_decl.type, level + 1);
    }
    if (node->as.var_decl.init) {
      indent(level + 1);
      printf("Init:\n");
      ast_print_ext(node->as.var_decl.init, level + 2);
    }
    break;

  case AST_TYPE_DECL:
    printf("TypeDecl name='%s' is_pub=%s\n",
           node->as.type_decl.name ? node->as.type_decl.name : "(null)",
           node->as.type_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.type_decl.fields, level + 1);
    break;

  case AST_VARIANT_DECL:
    printf("VariantDecl name='%s' is_pub=%s\n",
           node->as.variant_decl.name ? node->as.variant_decl.name : "(null)",
           node->as.variant_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.variant_decl.arms, level + 1);
    break;

  case AST_VARIANT_ARM:
    printf("VariantArm name='%s'\n",
           node->as.variant_arm.name ? node->as.variant_arm.name : "(null)");
    ast_print_ext(node->as.variant_arm.fields, level + 1);
    break;

  case AST_SCHEMA_DECL:
    printf("SchemaDecl name='%s' parent='%s' is_pub=%s\n",
           node->as.schema_decl.name ? node->as.schema_decl.name : "(null)",
           node->as.schema_decl.parent ? node->as.schema_decl.parent : "(null)",
           node->as.schema_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.schema_decl.fields, level + 1);
    break;

  case AST_FIELD_DECL:
    printf("FieldDecl name='%s' is_volatile=%s\n",
           node->as.field_decl.name ? node->as.field_decl.name : "(null)",
           node->as.field_decl.is_volatile ? "true" : "false");
    ast_print_ext(node->as.field_decl.type, level + 1);
    if (node->as.field_decl.default_val) {
      indent(level + 1);
      printf("Default:\n");
      ast_print_ext(node->as.field_decl.default_val, level + 2);
    }
    break;

  case AST_METHOD_DECL:
    printf("MethodDecl type_name='%s' iface_name='%s' is_pub=%s\n",
           node->as.method_decl.type_name ? node->as.method_decl.type_name
                                          : "(null)",
           node->as.method_decl.iface_name ? node->as.method_decl.iface_name
                                           : "(null)",
           node->as.method_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.method_decl.methods, level + 1);
    break;

  case AST_INTERFACE_DECL:
    printf("InterfaceDecl name='%s' is_pub=%s\n",
           node->as.interface_decl.name ? node->as.interface_decl.name
                                        : "(null)",
           node->as.interface_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.interface_decl.methods, level + 1);
    break;

  case AST_ERROR_DECL:
    printf("ErrorDecl name='%s' is_pub=%s\n",
           node->as.error_decl.name ? node->as.error_decl.name : "(null)",
           node->as.error_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.error_decl.variants, level + 1);
    break;

  case AST_MOD_DECL:
    printf("ModDecl name='%s' is_pub=%s\n",
           node->as.mod_decl.name ? node->as.mod_decl.name : "(null)",
           node->as.mod_decl.is_pub ? "true" : "false");
    ast_print_ext(node->as.mod_decl.declarations, level + 1);
    break;

  case AST_USE_DECL:
    printf("UseDecl\n");
    ast_print_ext(node->as.use_decl.path, level + 1);
    break;

  case AST_EXTERN_DECL:
    printf("ExternDecl name='%s' is_func=%s\n",
           node->as.extern_decl.name ? node->as.extern_decl.name : "(null)",
           node->as.extern_decl.is_func ? "true" : "false");
    if (node->as.extern_decl.is_func) {
      if (node->as.extern_decl.params) {
        indent(level + 1);
        printf("Params:\n");
        ast_print_ext(node->as.extern_decl.params, level + 2);
      }
      if (node->as.extern_decl.ret_name) {
        indent(level + 1);
        printf("Return name='%s'\n", node->as.extern_decl.ret_name);
        ast_print_ext(node->as.extern_decl.ret_type, level + 2);
      }
    } else {
      ast_print_ext(node->as.extern_decl.var_type, level + 1);
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
    printf("Identifier name='%s'\n",
           node->as.identifier.name ? node->as.identifier.name : "(null)");
    break;

  case AST_INT_LITERAL:
    printf("IntLiteral value=%llu\n", node->as.int_literal.value);
    break;

  case AST_FLOAT_LITERAL:
    printf("FloatLiteral value=%f\n", node->as.float_literal.value);
    break;

  case AST_STRING_LITERAL:
    printf("StringLiteral value=\"%s\"\n", node->as.string_literal.value
                                               ? node->as.string_literal.value
                                               : "(null)");
    break;

  case AST_BOOL_LITERAL:
    printf("BoolLiteral value=%s\n",
           node->as.bool_literal.value ? "true" : "false");
    break;

  case AST_CHAR_LITERAL:
    printf("CharLiteral codepoint=U+%04X\n", node->as.char_literal.codepoint);
    break;

  case AST_TYPE_EXPR:
    printf("TypeExpr kind=%s", type_kind_to_string(node->as.type_expr.kind));
    if (node->as.type_expr.kind == TYPE_NAMED) {
      printf(" name='%s'",
             node->as.type_expr.name ? node->as.type_expr.name : "(null)");
    } else if (node->as.type_expr.kind == TYPE_QUALIFIED) {
      printf(" module='%s' name='%s'",
             node->as.type_expr.module ? node->as.type_expr.module : "(null)",
             node->as.type_expr.name ? node->as.type_expr.name : "(null)");
    }
    printf("\n");
    if (node->as.type_expr.inner) {
      ast_print_ext(node->as.type_expr.inner, level + 1);
    }
    if (node->as.type_expr.size) {
      indent(level + 1);
      printf("Size:\n");
      ast_print_ext(node->as.type_expr.size, level + 2);
    }
    if (node->as.type_expr.elems) {
      ast_print_ext(node->as.type_expr.elems, level + 1);
    }
    break;

  case AST_CALL_EXPR:
    printf("CallExpr\n");
    indent(level + 1);
    printf("Callee:\n");
    ast_print_ext(node->as.call.callee, level + 2);
    if (node->as.call.args) {
      indent(level + 1);
      printf("Args:\n");
      ast_print_ext(node->as.call.args, level + 2);
    }
    break;

  case AST_IF_STMT:
    printf("IfStmt\n");
    indent(level + 1);
    printf("Condition:\n");
    ast_print_ext(node->as.if_stmt.condition, level + 2);
    indent(level + 1);
    printf("Then:\n");
    ast_print_ext(node->as.if_stmt.then_branch, level + 2);
    if (node->as.if_stmt.else_branch) {
      indent(level + 1);
      printf("Else:\n");
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
    indent(level + 1);
    printf("Target:\n");
    ast_print_ext(node->as.assign.target, level + 2);
    indent(level + 1);
    printf("Value:\n");
    ast_print_ext(node->as.assign.value, level + 2);
    break;

  case AST_FIELD_EXPR:
    printf("FieldExpr field='%s'\n",
           node->as.field.field ? node->as.field.field : "(null)");
    ast_print_ext(node->as.field.target, level + 1);
    break;

  case AST_INDEX_EXPR:
    printf("IndexExpr\n");
    indent(level + 1);
    printf("Target:\n");
    ast_print_ext(node->as.index.target, level + 2);
    indent(level + 1);
    printf("Index:\n");
    ast_print_ext(node->as.index.index, level + 2);
    break;

  case AST_CAST_EXPR:
    printf("CastExpr kind=%d\n", node->as.cast.kind);
    indent(level + 1);
    printf("Expr:\n");
    ast_print_ext(node->as.cast.expr, level + 2);
    indent(level + 1);
    printf("Target Type:\n");
    ast_print_ext(node->as.cast.target_type, level + 2);
    break;

  case AST_PROMOTE_EXPR:
    printf("PromoteExpr target=%s\n", realm_to_string(node->as.promote.target));
    ast_print_ext(node->as.promote.expr, level + 1);
    break;

  case AST_SIZEOF_EXPR:
    printf("SizeofExpr\n");
    indent(level + 1);
    printf("Type:\n");
    ast_print_ext(node->as.sizeof_expr.type, level + 2);
    break;

  case AST_ALIGNOF_EXPR:
    printf("AlignofExpr\n");
    indent(level + 1);
    printf("Type:\n");
    ast_print_ext(node->as.alignof_expr.type, level + 2);
    break;

  case AST_MATCH_STMT:
    printf("MatchStmt\n");
    indent(level + 1);
    printf("Subject:\n");
    ast_print_ext(node->as.match_stmt.subject, level + 2);
    indent(level + 1);
    printf("Arms:\n");
    ast_print_ext(node->as.match_stmt.arms, level + 2);
    break;

  case AST_MATCH_ARM:
    printf("MatchArm\n");
    indent(level + 1);
    printf("Pattern:\n");
    ast_print_ext(node->as.match_arm.pattern, level + 2);
    if (node->as.match_arm.guard) {
      indent(level + 1);
      printf("Guard:\n");
      ast_print_ext(node->as.match_arm.guard, level + 2);
    }
    indent(level + 1);
    printf("Body:\n");
    ast_print_ext(node->as.match_arm.body, level + 2);
    break;

  case AST_FOR_STMT:
    printf("ForStmt cap_kind=%d cap_value='%s' cap_index='%s'\n",
           node->as.for_stmt.cap_kind,
           node->as.for_stmt.cap_value ? node->as.for_stmt.cap_value : "(null)",
           node->as.for_stmt.cap_index ? node->as.for_stmt.cap_index
                                       : "(null)");
    indent(level + 1);
    printf("Iter:\n");
    ast_print_ext(node->as.for_stmt.iter, level + 2);
    indent(level + 1);
    printf("Body:\n");
    ast_print_ext(node->as.for_stmt.body, level + 2);
    break;

  case AST_WHILE_STMT:
    printf("WhileStmt\n");
    indent(level + 1);
    printf("Condition:\n");
    ast_print_ext(node->as.while_stmt.condition, level + 2);
    indent(level + 1);
    printf("Body:\n");
    ast_print_ext(node->as.while_stmt.body, level + 2);
    break;

  case AST_LOOP_STMT:
    printf("LoopStmt\n");
    indent(level + 1);
    printf("Body:\n");
    ast_print_ext(node->as.loop_stmt.body, level + 2);
    break;

  case AST_BREAK_STMT:
    printf("BreakStmt\n");
    break;

  case AST_CONTINUE_STMT:
    printf("ContinueStmt\n");
    break;

  case AST_UNSAFE_BLOCK:
    printf("UnsafeBlock\n");
    indent(level + 1);
    printf("Body:\n");
    ast_print_ext(node->as.unsafe_block.body, level + 2);
    break;

  case AST_ARRAY_LITERAL:
    printf("ArrayLiteral\n");
    ast_print_ext(node->as.array_literal.elems, level + 1);
    break;

  case AST_TUPLE_EXPR:
    printf("TupleExpr\n");
    ast_print_ext(node->as.tuple_expr.elems, level + 1);
    break;

  case AST_NAMED_ARG:
    printf("NamedArg name='%s'\n",
           node->as.named_arg.name ? node->as.named_arg.name : "(null)");
    ast_print_ext(node->as.named_arg.value, level + 1);
    break;

  case AST_TUPLE_DESTRUCTURE:
    printf("TupleDestructure\n");
    if (node->as.tuple_destructure.targets) {
      indent(level + 1);
      printf("Targets:\n");
      ast_print_ext(node->as.tuple_destructure.targets, level + 2);
    }
    if (node->as.tuple_destructure.init) {
      indent(level + 1);
      printf("Init:\n");
      ast_print_ext(node->as.tuple_destructure.init, level + 2);
    }
    break;

  case AST_STRUCT_PATTERN:
    printf("StructPattern name='%s'\n",
           node->as.struct_pattern.name ? node->as.struct_pattern.name : "(null)");
    if (node->as.struct_pattern.fields) {
      indent(level + 1);
      printf("Fields:\n");
      ast_print_ext(node->as.struct_pattern.fields, level + 2);
    }
    break;

  case AST_FIELD_PATTERN:
    printf("FieldPattern name='%s'\n",
           node->as.field_pattern.name ? node->as.field_pattern.name : "(null)");
    if (node->as.field_pattern.pattern) {
      indent(level + 1);
      printf("Pattern:\n");
      ast_print_ext(node->as.field_pattern.pattern, level + 2);
    }
    break;

  case AST_TRY_EXPR:
    printf("TryExpr\n");
    ast_print_ext(node->as.try_expr.expr, level + 1);
    break;

  case AST_CATCH_EXPR:
    printf("CatchExpr err_name='%s'\n", node->as.catch_expr.err_name
                                            ? node->as.catch_expr.err_name
                                            : "(null)");
    indent(level + 1);
    printf("Expr:\n");
    ast_print_ext(node->as.catch_expr.expr, level + 2);
    indent(level + 1);
    printf("Handler:\n");
    ast_print_ext(node->as.catch_expr.handler, level + 2);
    break;

  case AST_ERROR_EXPR:
    printf("ErrorExpr\n");
    ast_print_ext(node->as.error_expr.path, level + 1);
    break;

  case AST_ASM_EXPR:
    printf("AsmExpr code='%s' output='%s'\n",
           node->as.asm_expr.code ? node->as.asm_expr.code : "(null)",
           node->as.asm_expr.output ? node->as.asm_expr.output : "(none)");
    break;

  case AST_VOLATILE_EXPR:
    printf("VolatileExpr\n");
    ast_print_ext(node->as.volatile_expr.expr, level + 1);
    break;

  case AST_RANGE_EXPR:
    printf("RangeExpr inclusive=%s\n",
           node->as.range_expr.inclusive ? "true" : "false");
    indent(level + 1);
    printf("Start:\n");
    ast_print_ext(node->as.range_expr.start, level + 2);
    indent(level + 1);
    printf("End:\n");
    ast_print_ext(node->as.range_expr.end, level + 2);
    break;

  default:
    printf("Node kind=%d (printing not fully implemented)\n", node->kind);
    break;
  }

  if (node->next) {
    ast_print_ext(node->next, level);
  }
}

void ast_print(AstNode *node) { ast_print_ext(node, 0); }
