CC = gcc
CFLAGS = -Isrc -std=c11 -Wall -Wextra -g

CORE_SRCS = src/lexer.c src/parser.c src/ast.c src/monomorphize.c src/project.c src/utils/arena.c src/utils/strtab.c src/tools/ast_print.c src/symbol_table.c src/resolver.c src/types.c src/typecheck.c src/codegen.c
MAIN_SRC = src/main.c
TARGET = runes

all: $(TARGET)

guide:
	@printf '%s\n' 'See docs/language-guide.md'

install-zed:
	bash editors/zed/install.bash
	bash editors/zed/install-icons.bash

$(TARGET): $(CORE_SRCS) $(MAIN_SRC)
	$(CC) $(CFLAGS) $(CORE_SRCS) $(MAIN_SRC) -o $(TARGET)

LEXER_TEST = /tmp/runes_lexer_test
PARSER_TEST = /tmp/runes_parser_test
ARENA_TEST = /tmp/runes_arena_test
RUNTIME_TEST = /tmp/runes_runtime_test
FUZZ_FRONTEND = /tmp/runes_fuzz_frontend
FUZZ_SRCS = src/tests/fuzz_frontend.c src/lexer.c src/parser.c src/ast.c src/monomorphize.c src/utils/arena.c src/utils/strtab.c src/symbol_table.c src/resolver.c src/types.c src/typecheck.c src/codegen.c

test-unit: $(TARGET)
	$(CC) $(CFLAGS) src/tests/arena_test.c src/utils/arena.c -o $(ARENA_TEST)
	$(ARENA_TEST)
	$(CC) $(CFLAGS) src/tests/runtime_test.c src/runtime.c src/utils/arena.c -o $(RUNTIME_TEST)
	$(RUNTIME_TEST)
	$(CC) $(CFLAGS) src/tests/lexer_test.c src/lexer.c src/utils/arena.c src/utils/strtab.c -o $(LEXER_TEST)
	$(LEXER_TEST)
	$(CC) $(CFLAGS) src/tests/parser_test.c src/lexer.c src/parser.c src/ast.c src/utils/arena.c src/utils/strtab.c -o $(PARSER_TEST)
	$(PARSER_TEST)

test-core: $(TARGET)
	./$(TARGET) src/tests/samples/core_print_builtin.runes --emit-c /tmp/runes_core_print.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_print.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_print
	/tmp/runes_core_print | grep -E '^value423\.5truex0x[0-9a-fA-F]+$$'
	./$(TARGET) src/tests/samples/core_variant_payloads.runes
	./$(TARGET) src/tests/samples/core_bitwise.runes
	./$(TARGET) src/tests/samples/core_void_pointer.runes
	./$(TARGET) src/tests/samples/core_modules.runes
	./$(TARGET) src/tests/samples/core_use_import.runes
	./$(TARGET) src/tests/samples/core_inferred_scope.runes
	./$(TARGET) src/tests/samples/core_codegen_forward.runes --emit-c /tmp/runes_core_forward.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_forward.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_forward
	/tmp/runes_core_forward
	./$(TARGET) src/tests/samples/core_codegen_control.runes --emit-c /tmp/runes_core_control.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_control.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_control
	@test "$$(/tmp/runes_core_control)" = "23"
	./$(TARGET) src/tests/samples/core_codegen_struct.runes --emit-c /tmp/runes_core_struct.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_struct.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_struct
	@test "$$(/tmp/runes_core_struct)" = "43"
	./$(TARGET) src/tests/samples/core_codegen_inference.runes --emit-c /tmp/runes_core_inference.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_inference.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_inference
	@test "$$(/tmp/runes_core_inference)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_array_copy.runes --emit-c /tmp/runes_core_array_copy.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_array_copy.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_array_copy
	@test "$$(/tmp/runes_core_array_copy)" = "60"
	./$(TARGET) src/tests/samples/core_codegen_extern.runes --emit-c /tmp/runes_core_extern.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_extern.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_extern
	@test "$$(/tmp/runes_core_extern)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_globals.runes --emit-c /tmp/runes_core_globals.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_globals.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_globals
	@test "$$(/tmp/runes_core_globals)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_variants.runes --emit-c /tmp/runes_core_variants.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_variants.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_variants
	@test "$$(/tmp/runes_core_variants)" = "$$(printf '0 7 60\n10 20 30')"
	./$(TARGET) src/tests/samples/core_codegen_errors.runes --emit-c /tmp/runes_core_errors.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_errors.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_errors
	@test "$$(/tmp/runes_core_errors)" = "$$(printf '10 -1 -1\nerror 1\n3 -2')"
	./$(TARGET) src/tests/samples/core_codegen_methods.runes --emit-c /tmp/runes_core_methods.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_methods.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_methods
	@test "$$(/tmp/runes_core_methods)" = "17 17"
	./$(TARGET) src/tests/samples/core_codegen_modules.runes --emit-c /tmp/runes_core_modules.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_modules.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_modules
	@test "$$(/tmp/runes_core_modules)" = "20 42"
	./$(TARGET) src/tests/samples/core_codegen_module_name_collision.runes --emit-c /tmp/runes_core_module_name_collision.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_module_name_collision.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_module_name_collision
	@test "$$(/tmp/runes_core_module_name_collision)" = "20 22"
	./$(TARGET) src/tests/module_fixtures/flat/main.runes --emit-c /tmp/runes_core_modules_flat.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_modules_flat.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_modules_flat
	@test "$$(/tmp/runes_core_modules_flat)" = "42 42"
	./$(TARGET) src/tests/module_fixtures/directory/main.runes --emit-c /tmp/runes_core_modules_directory.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_modules_directory.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_modules_directory
	@test "$$(/tmp/runes_core_modules_directory)" = "42"
	@if ./$(TARGET) src/tests/module_fixtures/ambiguous/main.runes >/tmp/runes_module_ambiguous.out 2>&1; then \
		echo 'expected ambiguous filesystem module to fail'; exit 1; \
	fi
	@grep -Fq "Module 'item' is ambiguous" /tmp/runes_module_ambiguous.out
	./$(TARGET) src/tests/samples/core_arrays_pointers.runes --emit-c /tmp/runes_core_arrays_pointers.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_arrays_pointers.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_arrays_pointers
	/tmp/runes_core_arrays_pointers
	./$(TARGET) src/tests/samples/core_codegen_systems.runes --emit-c /tmp/runes_core_systems.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_systems.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_systems
	@test "$$(/tmp/runes_core_systems)" = "16 8 9 42"
	./$(TARGET) src/tests/samples/core_codegen_systems_attributes.runes --emit-c /tmp/runes_core_systems_attributes.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_systems_attributes.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_systems_attributes
	@test "$$(/tmp/runes_core_systems_attributes)" = "42 16 16 2 42 42"
	@nm /tmp/runes_core_systems_attributes | grep -Fq 'runes_exported_answer'
	@nm /tmp/runes_core_systems_attributes | grep -Fq 'runes_exported_counter'
	@if ./$(TARGET) src/tests/codegen_failures/interrupt.runes --emit-c /tmp/runes_interrupt.c >/tmp/runes_interrupt.out 2>&1; then \
		echo 'expected unsupported interrupt lowering to fail'; exit 1; \
	fi
	@grep -Fq '#[interrupt] is not supported by the v0.1 C backend' /tmp/runes_interrupt.out
	./$(TARGET) src/tests/samples/core_codegen_promote.runes --emit-c /tmp/runes_core_promote.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_promote.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote
	@test "$$(/tmp/runes_core_promote)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_deep_promotion.runes --emit-c /tmp/runes_core_deep_promotion.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_deep_promotion.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_deep_promotion
	@test "$$(/tmp/runes_core_deep_promotion)" = "true true 99 3"
	./$(TARGET) src/tests/samples/core_codegen_promote_aggregates.runes --emit-c /tmp/runes_core_promote_aggregates.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_promote_aggregates.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote_aggregates
	@test "$$(/tmp/runes_core_promote_aggregates)" = "$$(printf 'true true 42\ntrue')"
	./$(TARGET) src/tests/samples/core_codegen_promote_slices.runes --emit-c /tmp/runes_core_promote_slices.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_promote_slices.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote_slices
	@test "$$(/tmp/runes_core_promote_slices)" = "40 77 3 true 91"
	./$(TARGET) src/tests/samples/core_codegen_regional_arena.runes --emit-c /tmp/runes_core_regional_arena.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_regional_arena.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_regional_arena
	@test "$$(/tmp/runes_core_regional_arena)" = "42 43"
	./$(TARGET) src/tests/samples/core_codegen_regional_fallible_cleanup.runes --emit-c /tmp/runes_core_regional_fallible_cleanup.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_regional_fallible_cleanup.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_regional_fallible_cleanup
	@test "$$(/tmp/runes_core_regional_fallible_cleanup)" = "42 -1 0 0"
	./$(TARGET) src/tests/samples/core_codegen_provenance_long_chain.runes --emit-c /tmp/runes_core_provenance_long_chain.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_provenance_long_chain.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_provenance_long_chain
	@test "$$(/tmp/runes_core_provenance_long_chain)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_if_values.runes --emit-c /tmp/runes_core_if_values.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_if_values.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_if_values
	@test "$$(/tmp/runes_core_if_values)" = "global negative 42"
	./$(TARGET) src/tests/samples/core_codegen_strings.runes --emit-c /tmp/runes_core_strings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_strings.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_strings
	@test "$$(/tmp/runes_core_strings)" = "runes equal true true"
	./$(TARGET) src/tests/samples/core_codegen_c_string_cast.runes --emit-c /tmp/runes_core_c_string_cast.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_c_string_cast.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_c_string_cast
	@test "$$(/tmp/runes_core_c_string_cast)" = "hé 3"
	./$(TARGET) src/tests/samples/core_codegen_unicode_strings.runes --emit-c /tmp/runes_core_unicode_strings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_unicode_strings.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_unicode_strings
	@test "$$(/tmp/runes_core_unicode_strings)" = "13 true true 108 hé 界"
	./$(TARGET) src/tests/samples/core_string_ordering.runes --emit-c /tmp/runes_core_string_ordering.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_string_ordering.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_string_ordering
	@test "$$(/tmp/runes_core_string_ordering)" = "true true true true"
	./$(TARGET) src/tests/samples/core_codegen_string_bounds_trap.runes --emit-c /tmp/runes_core_string_bounds.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_string_bounds.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_string_bounds
	@if /tmp/runes_core_string_bounds >/tmp/runes_core_string_bounds.out 2>&1; then \
		echo 'expected string bounds check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes runtime error at 4:16: string byte index out of bounds' /tmp/runes_core_string_bounds.out
	./$(TARGET) src/tests/samples/core_codegen_string_boundary_trap.runes --emit-c /tmp/runes_core_string_boundary.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_string_boundary.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_string_boundary
	@if /tmp/runes_core_string_boundary >/tmp/runes_core_string_boundary.out 2>&1; then \
		echo 'expected UTF-8 boundary check to trap'; exit 1; \
	fi
	@grep -Fq 'string byte range splits a UTF-8 scalar' /tmp/runes_core_string_boundary.out
	./$(TARGET) src/tests/samples/core_codegen_char_cast.runes --emit-c /tmp/runes_core_char_cast.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_char_cast.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_char_cast
	@test "$$(/tmp/runes_core_char_cast)" = "🌍 127757"
	./$(TARGET) src/tests/samples/core_codegen_char_cast_trap.runes --emit-c /tmp/runes_core_char_cast_trap.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_char_cast_trap.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_char_cast_trap
	@if /tmp/runes_core_char_cast_trap >/tmp/runes_core_char_cast_trap.out 2>&1; then \
		echo 'expected invalid Unicode scalar cast to trap'; exit 1; \
	fi
	@grep -Fq 'integer is not a Unicode scalar' /tmp/runes_core_char_cast_trap.out
	./$(TARGET) src/tests/samples/core_codegen_named_early_return.runes --emit-c /tmp/runes_core_early_return.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_early_return.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_early_return
	@test "$$(/tmp/runes_core_early_return)" = "2 -1"
	./$(TARGET) src/tests/samples/core_codegen_nested_functions.runes --emit-c /tmp/runes_core_nested.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_nested.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_nested
	@test "$$(/tmp/runes_core_nested)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_nested_captures.runes --emit-c /tmp/runes_core_nested_captures.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_nested_captures.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_nested_captures
	@test "$$(/tmp/runes_core_nested_captures)" = "15 42 18"
	./$(TARGET) src/tests/samples/core_codegen_function_values.runes --emit-c /tmp/runes_core_function_values.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_function_values.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_function_values
	@test "$$(/tmp/runes_core_function_values)" = "42 20 42 14 16 18 6 1 18 22 1"
	./$(TARGET) src/tests/samples/core_codegen_move_closure.runes --emit-c /tmp/runes_core_move_closure.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_move_closure.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_move_closure
	@test "$$(/tmp/runes_core_move_closure)" = "42 45 40 40"
	./$(TARGET) src/tests/samples/core_codegen_promote_closure.runes --emit-c /tmp/runes_core_promote_closure.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_promote_closure.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote_closure
	@test "$$(/tmp/runes_core_promote_closure)" = "42 45"
	./$(TARGET) src/tests/samples/core_codegen_gc_move_closure.runes --emit-c /tmp/runes_core_gc_move_closure.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_move_closure.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_move_closure
	@test "$$(/tmp/runes_core_gc_move_closure)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_gc_graph.runes --emit-c /tmp/runes_core_gc_graph.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_graph.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_graph
	@test "$$(/tmp/runes_core_gc_graph)" = "1 2 3 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_typed_sequence.runes --emit-c /tmp/runes_core_gc_typed_sequence.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_typed_sequence.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_typed_sequence
	@test "$$(/tmp/runes_core_gc_typed_sequence)" = "42 2"
	./$(TARGET) src/tests/samples/core_codegen_promote_gc.runes --emit-c /tmp/runes_core_promote_gc.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_promote_gc.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote_gc
	@test "$$(/tmp/runes_core_promote_gc)" = "true true 42 2"
	./$(TARGET) src/tests/samples/core_codegen_promote_gc_slice.runes --emit-c /tmp/runes_core_promote_gc_slice.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_promote_gc_slice.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_promote_gc_slice
	@test "$$(/tmp/runes_core_promote_gc_slice)" = "true 42 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_flex_global.runes --emit-c /tmp/runes_core_gc_flex_global.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_flex_global.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_flex_global
	@test "$$(/tmp/runes_core_gc_flex_global)" = "77 1"
	./$(TARGET) src/tests/samples/core_codegen_gc_temporaries.runes --emit-c /tmp/runes_core_gc_temporaries.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_temporaries.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_temporaries
	@test "$$(/tmp/runes_core_gc_temporaries)" = "42 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_control_bindings.runes --emit-c /tmp/runes_core_gc_control_bindings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -Werror /tmp/runes_core_gc_control_bindings.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_gc_control_bindings
	@test "$$(/tmp/runes_core_gc_control_bindings)" = "42 43"
	./$(TARGET) src/tests/samples/core_codegen_interfaces.runes --emit-c /tmp/runes_core_interfaces.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_interfaces.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_interfaces
	@test "$$(/tmp/runes_core_interfaces)" = "7 9"
	./$(TARGET) src/tests/samples/core_codegen_method_name_collision.runes --emit-c /tmp/runes_core_method_collision.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_method_collision.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_method_collision
	@test "$$(/tmp/runes_core_method_collision)" = "41 42"
	./$(TARGET) src/tests/samples/core_codegen_nullable_pointers.runes --emit-c /tmp/runes_core_nullable.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_nullable.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_nullable
	@test "$$(/tmp/runes_core_nullable)" = "true true"
	./$(TARGET) src/tests/samples/core_codegen_nullable_unwrap.runes --emit-c /tmp/runes_core_nullable_unwrap.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_nullable_unwrap.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_nullable_unwrap
	@test "$$(/tmp/runes_core_nullable_unwrap)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_nested_pointer_types.runes --emit-c /tmp/runes_core_nested_pointer_types.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_nested_pointer_types.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_nested_pointer_types
	@test "$$(/tmp/runes_core_nested_pointer_types)" = "42 42 true"
	./$(TARGET) src/tests/samples/core_codegen_null_unwrap_trap.runes --emit-c /tmp/runes_core_null_unwrap.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_null_unwrap.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_null_unwrap
	@if /tmp/runes_core_null_unwrap >/tmp/runes_core_null_unwrap.out 2>&1; then \
		echo 'expected null unwrap check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes null error at 3:17: attempted to unwrap a null pointer' /tmp/runes_core_null_unwrap.out
	./$(TARGET) src/tests/samples/core_codegen_bounds_trap.runes --emit-c /tmp/runes_core_bounds.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_bounds.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_bounds
	@if /tmp/runes_core_bounds >/tmp/runes_core_bounds.out 2>&1; then \
		echo 'expected dynamic bounds check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes bounds error at 4:17: index 2, length 2' /tmp/runes_core_bounds.out
	./$(TARGET) src/tests/samples/core_codegen_overflow_trap.runes --emit-c /tmp/runes_core_overflow.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_overflow.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_overflow
	@if /tmp/runes_core_overflow >/tmp/runes_core_overflow.out 2>&1; then \
		echo 'expected integer overflow check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes arithmetic error at 4:19: overflow in addition' /tmp/runes_core_overflow.out
	./$(TARGET) src/tests/samples/core_codegen_checked_shifts.runes --emit-c /tmp/runes_core_checked_shifts.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_checked_shifts.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_checked_shifts
	@test "$$(/tmp/runes_core_checked_shifts)" = "-2 12"
	./$(TARGET) src/tests/samples/core_codegen_narrow_integer_ops.runes --emit-c /tmp/runes_core_narrow_integer_ops.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_narrow_integer_ops.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_narrow_integer_ops
	@test "$$(/tmp/runes_core_narrow_integer_ops)" = "255 255 255 0"
	./$(TARGET) src/tests/samples/core_codegen_multiline_delimiters.runes --emit-c /tmp/runes_core_multiline_delimiters.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_multiline_delimiters.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_multiline_delimiters
	@test "$$(/tmp/runes_core_multiline_delimiters)" = "424242"
	./$(TARGET) src/tests/samples/core_codegen_slices.runes --emit-c /tmp/runes_core_slices.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_slices.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_slices
	@test "$$(/tmp/runes_core_slices)" = "4 2 21 101 101"
	./$(TARGET) src/tests/samples/core_codegen_slice_aggregates.runes --emit-c /tmp/runes_core_slice_aggregates.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_slice_aggregates.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_slice_aggregates
	@test "$$(/tmp/runes_core_slice_aggregates)" = "$$(printf '20 3\n28')"
	./$(TARGET) src/tests/samples/core_codegen_slice_methods.runes --emit-c /tmp/runes_core_slice_methods.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_slice_methods.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_slice_methods
	@test "$$(/tmp/runes_core_slice_methods)" = "62 62"
	./$(TARGET) src/tests/samples/core_codegen_raw_slice.runes --emit-c /tmp/runes_core_raw_slice.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_raw_slice.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_raw_slice
	@test "$$(/tmp/runes_core_raw_slice)" = "42 3"
	./$(TARGET) src/tests/samples/core_codegen_zero_length_slice.runes --emit-c /tmp/runes_core_zero_slice.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_zero_slice.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_zero_slice
	@test "$$(/tmp/runes_core_zero_slice)" = "0"
	./$(TARGET) src/tests/samples/core_codegen_slice_bounds_trap.runes --emit-c /tmp/runes_core_slice_bounds.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_slice_bounds.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_slice_bounds
	@if /tmp/runes_core_slice_bounds >/tmp/runes_core_slice_bounds.out 2>&1; then \
		echo 'expected slice bounds check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes bounds error at 4:16: index 2, length 2' /tmp/runes_core_slice_bounds.out
	./$(TARGET) src/tests/samples/core_codegen_slice_range_trap.runes --emit-c /tmp/runes_core_slice_range.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_slice_range.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_slice_range
	@if /tmp/runes_core_slice_range >/tmp/runes_core_slice_range.out 2>&1; then \
		echo 'expected invalid slice range to trap'; exit 1; \
	fi
	@grep -Fq 'invalid slice range 2..1 for length 3' /tmp/runes_core_slice_range.out
	./$(TARGET) src/tests/samples/core_codegen_negation_trap.runes --emit-c /tmp/runes_core_negation.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_negation.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_negation
	@if /tmp/runes_core_negation >/tmp/runes_core_negation.out 2>&1; then \
		echo 'expected integer negation check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes arithmetic error at 3:11: overflow in negation' /tmp/runes_core_negation.out
	./$(TARGET) src/tests/samples/core_codegen_shift_overflow_trap.runes --emit-c /tmp/runes_core_shift_overflow.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_shift_overflow.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_shift_overflow
	@if /tmp/runes_core_shift_overflow >/tmp/runes_core_shift_overflow.out 2>&1; then \
		echo 'expected left-shift overflow check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes arithmetic error at 3:17: overflow in left shift' /tmp/runes_core_shift_overflow.out
	./$(TARGET) src/tests/samples/core_codegen_shift_count_trap.runes --emit-c /tmp/runes_core_shift_count.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_shift_count.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_shift_count
	@if /tmp/runes_core_shift_count >/tmp/runes_core_shift_count.out 2>&1; then \
		echo 'expected invalid shift count check to trap'; exit 1; \
	fi
	@grep -Fq 'Runes arithmetic error at 4:17: invalid shift count' /tmp/runes_core_shift_count.out
	./$(TARGET) src/tests/samples/core_codegen_array_return.runes --emit-c /tmp/runes_core_array_return.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_core_array_return.c src/runtime.c src/utils/arena.c -o /tmp/runes_core_array_return
	@test "$$(/tmp/runes_core_array_return)" = "42"
	./$(TARGET) src/tests/samples/test_error_flow.runes --emit-c /tmp/runes_error_flow.c
	$(CC) -Isrc -std=c11 -Wall -Wextra /tmp/runes_error_flow.c src/runtime.c src/utils/arena.c -o /tmp/runes_error_flow
	@test "$$(/tmp/runes_error_flow)" = "$$(printf 'caught: 1\n0\n2 0')"
	@for f in src/tests/samples/core_*_error.runes; do \
		pattern=$$(sed -n 's/^-- EXPECT FAIL: //p' "$$f"); \
		if ./$(TARGET) "$$f" >/tmp/runes_expected_error 2>&1; then \
			echo "expected failure compiled successfully: $$f"; exit 1; \
		fi; \
		if ! grep -Fq "$$pattern" /tmp/runes_expected_error; then \
			echo "missing expected diagnostic in $$f: $$pattern"; exit 1; \
		fi; \
	done

test-tooling: $(TARGET)
	./runec check src/examples/hello_codegen.runes
	./runec build src/examples/hello_codegen.runes -o /tmp/runes_tooling_test
	@test "$$(/tmp/runes_tooling_test)" = "hello world"
	./runec check src/examples/language_tour.runes
	./runec build src/examples/language_tour.runes -o /tmp/runes_language_tour
	@test "$$(/tmp/runes_language_tour)" = "42 42 42 0"
	cd src/tests/project_fixtures/workspace && ../../../../runec check
	cd src/tests/project_fixtures/workspace && ../../../../runec build -o /tmp/runes_workspace_project
	@test "$$(/tmp/runes_workspace_project)" = "42"
	@if cd src/tests/project_fixtures/cycle && ../../../../runec check >/tmp/runes_project_cycle.out 2>&1; then \
		echo 'expected cyclic project modules to fail'; exit 1; \
	fi
	@grep -Fq 'Cyclic module dependency:' /tmp/runes_project_cycle.out
	@if cd src/tests/project_fixtures/ambiguous_roots && ../../../../runec check >/tmp/runes_project_ambiguous.out 2>&1; then \
		echo 'expected ambiguous project modules to fail'; exit 1; \
	fi
	@grep -Fq "Module 'helper' is ambiguous across module roots" /tmp/runes_project_ambiguous.out
	@if cd src/tests/project_fixtures/invalid_manifest && ../../../../runec check >/tmp/runes_project_manifest.out 2>&1; then \
		echo 'expected unknown manifest field to fail'; exit 1; \
	fi
	@grep -Fq 'manifest error: unknown field' /tmp/runes_project_manifest.out

test-zed:
	bash editors/zed/test.bash

test: test-unit test-core test-tooling test-docs test-codegen-scale test-differential

test-docs: $(TARGET)
	bash docs/test.bash

test-codegen-scale: $(TARGET)
	bash src/tests/codegen_scale_test.bash

test-differential: $(TARGET)
	bash src/tests/codegen_differential_test.bash

test-samples: $(TARGET)
	bash src/tests/tester.bash

test-codegen: $(TARGET)
	bash src/tests/codegen_inventory.bash

test-runtime-sanitize: $(TARGET)
	./$(TARGET) src/tests/samples/core_codegen_deep_promotion.runes --emit-c /tmp/runes_asan_deep_promotion.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_deep_promotion.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_deep_promotion
	ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_deep_promotion >/dev/null
	./$(TARGET) src/tests/samples/core_codegen_promote_aggregates.runes --emit-c /tmp/runes_asan_promote_aggregates.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_aggregates.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_aggregates
	ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_aggregates >/dev/null
	./$(TARGET) src/tests/samples/core_codegen_promote_slices.runes --emit-c /tmp/runes_asan_promote_slices.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_slices.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_slices
	ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_slices >/dev/null
	./$(TARGET) src/tests/samples/core_codegen_promote_strings.runes --emit-c /tmp/runes_asan_promote_strings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_strings.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_strings
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_strings)" = "12 true"
	./$(TARGET) src/tests/samples/core_codegen_promote_closure.runes --emit-c /tmp/runes_asan_promote_closure.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_closure.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_closure
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_closure)" = "42 45"
	./$(TARGET) src/tests/samples/core_codegen_unicode_strings.runes --emit-c /tmp/runes_asan_unicode_strings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_unicode_strings.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_unicode_strings
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_unicode_strings)" = "13 true true 108 hé 界"
	./$(TARGET) src/tests/samples/core_codegen_regional_fallible_cleanup.runes --emit-c /tmp/runes_asan_regional_cleanup.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_regional_cleanup.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_regional_cleanup
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_regional_cleanup)" = "42 -1 0 0"
	./$(TARGET) src/tests/samples/core_codegen_gc_graph.runes --emit-c /tmp/runes_asan_gc_graph.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_gc_graph.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_gc_graph
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_gc_graph)" = "1 2 3 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_typed_sequence.runes --emit-c /tmp/runes_asan_gc_typed_sequence.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_gc_typed_sequence.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_gc_typed_sequence
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_gc_typed_sequence)" = "42 2"
	./$(TARGET) src/tests/samples/core_codegen_gc_temporaries.runes --emit-c /tmp/runes_asan_gc_temporaries.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_gc_temporaries.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_gc_temporaries
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_gc_temporaries)" = "42 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_control_bindings.runes --emit-c /tmp/runes_asan_gc_control_bindings.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_gc_control_bindings.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_gc_control_bindings
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_gc_control_bindings)" = "42 43"
	./$(TARGET) src/tests/samples/core_codegen_promote_gc.runes --emit-c /tmp/runes_asan_promote_gc.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_gc.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_gc
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_gc)" = "true true 42 2"
	./$(TARGET) src/tests/samples/core_codegen_promote_gc_slice.runes --emit-c /tmp/runes_asan_promote_gc_slice.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_promote_gc_slice.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_promote_gc_slice
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_promote_gc_slice)" = "true 42 3"
	./$(TARGET) src/tests/samples/core_codegen_gc_move_closure.runes --emit-c /tmp/runes_asan_gc_move_closure.c
	$(CC) -Isrc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/runes_asan_gc_move_closure.c src/runtime.c src/utils/arena.c -o /tmp/runes_asan_gc_move_closure
	@test "$$(ASAN_OPTIONS=detect_leaks=0 /tmp/runes_asan_gc_move_closure)" = "42"

test-sanitize: test-runtime-sanitize
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(CORE_SRCS) $(MAIN_SRC) -o /tmp/runes_sanitize
	@failures=0; for f in src/tests/samples/*.runes; do \
		if head -1 "$$f" | grep -q '^-- EXPECT FAIL: '; then context="$$f"; \
		elif ./$(TARGET) "$$f" >/dev/null 2>&1; then context="$$f"; \
		else context="src/tests/fixtures/sample_prelude.runes $$f"; fi; \
		ASAN_OPTIONS=detect_leaks=0 /tmp/runes_sanitize $$context >/tmp/runes_sanitize.out 2>&1 || true; \
		if grep -Eq 'AddressSanitizer|runtime error:' /tmp/runes_sanitize.out; then \
			echo "sanitizer failure: $$f"; sed -n '1,20p' /tmp/runes_sanitize.out; failures=$$((failures + 1)); \
		fi; \
	done; test $$failures -eq 0

fuzz-build:
	clang -Isrc -std=c11 -g -O1 -fno-omit-frame-pointer \
		-fsanitize=fuzzer,address,undefined $(FUZZ_SRCS) -o $(FUZZ_FRONTEND)

fuzz-smoke: fuzz-build
	@corpus=$$(mktemp -d /tmp/runes-fuzz-corpus.XXXXXX); \
		cp -R src/tests/fuzz_corpus/. "$$corpus"; \
		ASAN_OPTIONS=detect_leaks=0 $(FUZZ_FRONTEND) -runs=500 \
		-max_len=4096 -timeout=2 -artifact_prefix=/tmp/runes-fuzz-artifact- \
		"$$corpus"

fuzz: fuzz-build
	$(FUZZ_FRONTEND) -max_len=65536 -timeout=5 src/tests/fuzz_corpus

clean:
	rm -f $(TARGET) lexer_test.exe *.o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
