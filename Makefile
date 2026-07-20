CC = gcc
CFLAGS = -Isrc -std=c11 -Wall -Wextra -g

CORE_SRCS = src/lexer.c src/parser.c src/ast.c src/utils/arena.c src/utils/strtab.c src/tools/ast_print.c src/symbol_table.c src/resolver.c src/types.c src/typecheck.c src/codegen.c
MAIN_SRC = src/main.c
TARGET = runes

all: $(TARGET)

guide:
	@printf '%s\n' 'See docs/getting-started.md'

install-zed:
	bash editors/zed/install.bash

$(TARGET): $(CORE_SRCS) $(MAIN_SRC)
	$(CC) $(CFLAGS) $(CORE_SRCS) $(MAIN_SRC) -o $(TARGET)

LEXER_TEST = /tmp/runes_lexer_test
PARSER_TEST = /tmp/runes_parser_test

test-unit: $(TARGET)
	$(CC) $(CFLAGS) src/tests/lexer_test.c src/lexer.c src/utils/arena.c src/utils/strtab.c -o $(LEXER_TEST)
	$(LEXER_TEST)
	$(CC) $(CFLAGS) src/tests/parser_test.c src/lexer.c src/parser.c src/ast.c src/utils/arena.c src/utils/strtab.c -o $(PARSER_TEST)
	$(PARSER_TEST)

test-core: $(TARGET)
	./$(TARGET) src/tests/samples/core_print_builtin.runes --emit-c /tmp/runes_core_print.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_print.c -o /tmp/runes_core_print
	/tmp/runes_core_print | grep -E '^value 42 3\.5 true x 0x[0-9a-fA-F]+$$'
	./$(TARGET) src/tests/samples/core_variant_payloads.runes
	./$(TARGET) src/tests/samples/core_bitwise.runes
	./$(TARGET) src/tests/samples/core_void_pointer.runes
	./$(TARGET) src/tests/samples/core_modules.runes
	./$(TARGET) src/tests/samples/core_use_import.runes
	./$(TARGET) src/tests/samples/core_inferred_scope.runes
	./$(TARGET) src/tests/samples/core_codegen_forward.runes --emit-c /tmp/runes_core_forward.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_forward.c -o /tmp/runes_core_forward
	/tmp/runes_core_forward
	./$(TARGET) src/tests/samples/core_codegen_control.runes --emit-c /tmp/runes_core_control.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_control.c -o /tmp/runes_core_control
	@test "$$(/tmp/runes_core_control)" = "23"
	./$(TARGET) src/tests/samples/core_codegen_struct.runes --emit-c /tmp/runes_core_struct.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_struct.c -o /tmp/runes_core_struct
	@test "$$(/tmp/runes_core_struct)" = "43"
	./$(TARGET) src/tests/samples/core_codegen_inference.runes --emit-c /tmp/runes_core_inference.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_inference.c -o /tmp/runes_core_inference
	@test "$$(/tmp/runes_core_inference)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_array_copy.runes --emit-c /tmp/runes_core_array_copy.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_array_copy.c -o /tmp/runes_core_array_copy
	@test "$$(/tmp/runes_core_array_copy)" = "60"
	./$(TARGET) src/tests/samples/core_codegen_extern.runes --emit-c /tmp/runes_core_extern.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_extern.c -o /tmp/runes_core_extern
	@test "$$(/tmp/runes_core_extern)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_globals.runes --emit-c /tmp/runes_core_globals.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_globals.c -o /tmp/runes_core_globals
	@test "$$(/tmp/runes_core_globals)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_variants.runes --emit-c /tmp/runes_core_variants.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_variants.c -o /tmp/runes_core_variants
	@test "$$(/tmp/runes_core_variants)" = "$$(printf '0 7 60\n10 20 30')"
	./$(TARGET) src/tests/samples/core_codegen_errors.runes --emit-c /tmp/runes_core_errors.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_errors.c -o /tmp/runes_core_errors
	@test "$$(/tmp/runes_core_errors)" = "$$(printf '10 -1 -1\nerror 1\n3 -2')"
	./$(TARGET) src/tests/samples/core_codegen_methods.runes --emit-c /tmp/runes_core_methods.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_methods.c -o /tmp/runes_core_methods
	@test "$$(/tmp/runes_core_methods)" = "17 17"
	./$(TARGET) src/tests/samples/core_codegen_modules.runes --emit-c /tmp/runes_core_modules.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_modules.c -o /tmp/runes_core_modules
	@test "$$(/tmp/runes_core_modules)" = "20 42"
	./$(TARGET) src/tests/samples/core_arrays_pointers.runes --emit-c /tmp/runes_core_arrays_pointers.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_arrays_pointers.c -o /tmp/runes_core_arrays_pointers
	/tmp/runes_core_arrays_pointers
	./$(TARGET) src/tests/samples/core_codegen_systems.runes --emit-c /tmp/runes_core_systems.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_systems.c -o /tmp/runes_core_systems
	@test "$$(/tmp/runes_core_systems)" = "16 8 9 42"
	./$(TARGET) src/tests/samples/core_codegen_promote.runes --emit-c /tmp/runes_core_promote.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_promote.c -o /tmp/runes_core_promote
	@test "$$(/tmp/runes_core_promote)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_if_values.runes --emit-c /tmp/runes_core_if_values.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_if_values.c -o /tmp/runes_core_if_values
	@test "$$(/tmp/runes_core_if_values)" = "global negative 42"
	./$(TARGET) src/tests/samples/core_codegen_strings.runes --emit-c /tmp/runes_core_strings.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_strings.c -o /tmp/runes_core_strings
	@test "$$(/tmp/runes_core_strings)" = "runes equal true true"
	./$(TARGET) src/tests/samples/core_codegen_named_early_return.runes --emit-c /tmp/runes_core_early_return.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_early_return.c -o /tmp/runes_core_early_return
	@test "$$(/tmp/runes_core_early_return)" = "2 -1"
	./$(TARGET) src/tests/samples/core_codegen_nested_functions.runes --emit-c /tmp/runes_core_nested.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_nested.c -o /tmp/runes_core_nested
	@test "$$(/tmp/runes_core_nested)" = "42"
	./$(TARGET) src/tests/samples/core_codegen_interfaces.runes --emit-c /tmp/runes_core_interfaces.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_interfaces.c -o /tmp/runes_core_interfaces
	@test "$$(/tmp/runes_core_interfaces)" = "7 9"
	./$(TARGET) src/tests/samples/core_codegen_array_return.runes --emit-c /tmp/runes_core_array_return.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_core_array_return.c -o /tmp/runes_core_array_return
	@test "$$(/tmp/runes_core_array_return)" = "42"
	./$(TARGET) src/tests/samples/test_error_flow.runes --emit-c /tmp/runes_error_flow.c
	$(CC) -std=c11 -Wall -Wextra /tmp/runes_error_flow.c -o /tmp/runes_error_flow
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

test-zed:
	bash editors/zed/test.bash

test: test-unit test-core test-tooling

test-samples: $(TARGET)
	bash src/tests/tester.bash

test-codegen: $(TARGET)
	bash src/tests/codegen_inventory.bash

test-sanitize:
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

clean:
	rm -f $(TARGET) lexer_test.exe *.o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
