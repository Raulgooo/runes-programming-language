#include "../lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Define a simple test macro
#define ASSERT_TOKEN(L, expected_kind, expected_lexeme)                        \
  do {                                                                         \
    Token token = lexer_next_token(L);                                         \
    printf("[%s] '%.*s'\n", token_kind_to_string(token.kind),                  \
           (int)token.length, token.start);                                    \
    if (token.kind != expected_kind) {                                         \
      fprintf(stderr, "Test failed: line %d. Expected kind %s, got %s\n",      \
              __LINE__, token_kind_to_string(expected_kind),                   \
              token_kind_to_string(token.kind));                               \
      assert(token.kind == expected_kind);                                     \
    }                                                                          \
    if (strncmp(token.start, expected_lexeme, token.length) != 0 ||            \
        strlen(expected_lexeme) != token.length) {                             \
      fprintf(stderr,                                                          \
              "Test failed: line %d. Expected lexeme '%.*s', got '%.*s'\n",    \
              __LINE__, (int)strlen(expected_lexeme), expected_lexeme,         \
              (int)token.length, token.start);                                 \
      assert(0);                                                               \
    }                                                                          \
  } while (0)

void test_memory_scopes() {
  printf("Running test_memory_scopes...\n");
  Lexer L;
  // Probando todos los colores de memoria en firmas de funciones
  const char *source = "flex f poly() { regional f arena() {} }";
  lexer_init(&L, source);

  ASSERT_TOKEN(&L, TOKEN_FLEX, "flex");
  ASSERT_TOKEN(&L, TOKEN_F, "f");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "poly");
  ASSERT_TOKEN(&L, TOKEN_LPAREN, "(");
  ASSERT_TOKEN(&L, TOKEN_RPAREN, ")");
  ASSERT_TOKEN(&L, TOKEN_LBRACE, "{");

  ASSERT_TOKEN(&L, TOKEN_REGIONAL, "regional");
  ASSERT_TOKEN(&L, TOKEN_F, "f");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "arena");
  ASSERT_TOKEN(&L, TOKEN_LPAREN, "(");
  ASSERT_TOKEN(&L, TOKEN_RPAREN, ")");
  ASSERT_TOKEN(&L, TOKEN_LBRACE, "{");
  ASSERT_TOKEN(&L, TOKEN_RBRACE, "}");

  ASSERT_TOKEN(&L, TOKEN_RBRACE, "}");
  ASSERT_TOKEN(&L, TOKEN_EOF, "");
  printf("test_memory_scopes passed!\n");
}

void test_os_features() {
  printf("Running test_os_features...\n");
  Lexer L;
  // Probando atributos de linker, ffi y punteros volátiles
  const char *source = "#[section]\n"
                       "extern volatile *u32 uart = 0x1000;";
  lexer_init(&L, source);

  // #[section]
  ASSERT_TOKEN(&L, TOKEN_HASH, "#");
  ASSERT_TOKEN(&L, TOKEN_LBRACKET, "[");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "section");
  ASSERT_TOKEN(&L, TOKEN_RBRACKET, "]");
  ASSERT_TOKEN(&L, TOKEN_NEWLINE, "\n");

  // extern volatile *u32 uart = 0x1000;
  ASSERT_TOKEN(&L, TOKEN_EXTERN, "extern");
  ASSERT_TOKEN(&L, TOKEN_VOLATILE, "volatile");
  ASSERT_TOKEN(&L, TOKEN_STAR, "*");
  ASSERT_TOKEN(&L, TOKEN_U32, "u32");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "uart");
  ASSERT_TOKEN(&L, TOKEN_EQUAL, "=");
  ASSERT_TOKEN(&L, TOKEN_INT_LITERAL,
               "0x1000"); // Tu lexer parsea el 0x junto con el número
  ASSERT_TOKEN(&L, TOKEN_SEMICOLON, ";");

  ASSERT_TOKEN(&L, TOKEN_EOF, "");
  printf("test_os_features passed!\n");
}

void test_control_flow_and_errors() {
  printf("Running test_control_flow_and_errors...\n");
  Lexer L;
  // Probando lambdas, manejo de errores y match
  const char *source = "try div() catch |e| -> !";
  lexer_init(&L, source);

  ASSERT_TOKEN(&L, TOKEN_TRY, "try");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "div");
  ASSERT_TOKEN(&L, TOKEN_LPAREN, "(");
  ASSERT_TOKEN(&L, TOKEN_RPAREN, ")");

  ASSERT_TOKEN(&L, TOKEN_CATCH, "catch");
  ASSERT_TOKEN(&L, TOKEN_PIPE, "|");
  ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "e");
  ASSERT_TOKEN(&L, TOKEN_PIPE, "|");

  ASSERT_TOKEN(&L, TOKEN_ARROW, "->");
  ASSERT_TOKEN(&L, TOKEN_BANG, "!");

  ASSERT_TOKEN(&L, TOKEN_EOF, "");
  printf("test_control_flow_and_errors passed!\n");
}

void test_function() {
  Lexer L;
  const char *source = "dynamic f alloc_buf(size: u64) = ptr: *u8 {
      ptr = raw_alloc(size)

          -- caller must free raw_alloc
}
";
    lexer_init(&L, source);

ASSERT_TOKEN(&L, TOKEN_DYNAMIC, "dynamic");
ASSERT_TOKEN(&L, TOKEN_F, "f");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "alloc_buf");
ASSERT_TOKEN(&L, TOKEN_LPAREN, "(");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "size");
ASSERT_TOKEN(&L, TOKEN_COLON, ":");
ASSERT_TOKEN(&L, TOKEN_U64, "u64");
ASSERT_TOKEN(&L, TOKEN_RPAREN, ")");
ASSERT_TOKEN(&L, TOKEN_EQUAL, "=");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "ptr");
ASSERT_TOKEN(&L, TOKEN_COLON, ":");
ASSERT_TOKEN(&L, TOKEN_STAR, "*");
ASSERT_TOKEN(&L, TOKEN_U8, "u8");
ASSERT_TOKEN(&L, TOKEN_LBRACE, "{");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "ptr");
ASSERT_TOKEN(&L, TOKEN_EQUAL, "=");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "raw_alloc");
ASSERT_TOKEN(&L, TOKEN_LPAREN, "(");
ASSERT_TOKEN(&L, TOKEN_IDENTIFIER, "size");
ASSERT_TOKEN(&L, TOKEN_MINUS_MINUS, "--");
ASSERT_TOKEN(&L, TOKEN_RPAREN, ")");
ASSERT_TOKEN(&L, TOKEN_NEWLINE, "\n");
ASSERT_TOKEN(&L, TOKEN_SEMICOLON, ";");
ASSERT_TOKEN(&L, TOKEN_RBRACE, "}");

ASSERT_TOKEN(&L, TOKEN_EOF, "");
printf("test_function passed!\n");
}
int main() {
  printf("--- Starting Runes Lexer Tests ---\n");
  test_memory_scopes();
  test_os_features();
  test_control_flow_and_errors();
  printf("--- All tests passed successfully! ---\n");
  return 0;
}