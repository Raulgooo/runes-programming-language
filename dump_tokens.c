#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buffer = (char *)malloc(size + 1);
  fread(buffer, 1, size, file);
  buffer[size] = '\0';
  fclose(file);
  return buffer;
}

int main() {
  char *source = read_file("src/examples/memory.runes");
  if (!source)
    return 1;

  Lexer lexer;
  lexer_init(&lexer, source, NULL);
  Token t = lexer_next_token(&lexer);
  while (t.kind != TOKEN_EOF) {
    if (t.line >= 23 && t.line <= 26) {
      if (t.str_val.ptr) {
        printf("Line %d, Col %d: Kind %d, Str '%.*s'\n", t.line, t.column,
               t.kind, (int)t.length, t.str_val.ptr);
      } else {
        printf("Line %d, Col %d: Kind %d, Str '(null)'\n", t.line, t.column,
               t.kind);
      }
    }
    t = lexer_next_token(&lexer);
  }
  free(source);
  return 0; //
}
