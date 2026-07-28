#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "utils/arena.h"
#include "utils/strtab.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} Buffer;

typedef struct Document {
  char *uri;
  char *text;
  struct Document *next;
} Document;

typedef struct {
  uint32_t line;
  uint32_t column;
  char *message;
} Diagnostic;

typedef struct {
  Diagnostic *items;
  size_t count;
  size_t capacity;
} Diagnostics;

typedef struct {
  Arena arena;
  StrTab strings;
  Lexer lexer;
  Parser parser;
  AstNode *program;
  Diagnostics diagnostics;
} Analysis;

static Document *documents;

static char *copy_string(const char *text) {
  size_t length = strlen(text);
  char *copy = malloc(length + 1);
  if (copy)
    memcpy(copy, text, length + 1);
  return copy;
}

static bool buffer_reserve(Buffer *buffer, size_t extra) {
  if (buffer->length + extra + 1 <= buffer->capacity)
    return true;
  size_t capacity = buffer->capacity ? buffer->capacity : 256;
  while (capacity < buffer->length + extra + 1)
    capacity *= 2;
  char *grown = realloc(buffer->data, capacity);
  if (!grown)
    return false;
  buffer->data = grown;
  buffer->capacity = capacity;
  return true;
}

static bool buffer_append_n(Buffer *buffer, const char *text, size_t length) {
  if (!buffer_reserve(buffer, length))
    return false;
  memcpy(buffer->data + buffer->length, text, length);
  buffer->length += length;
  buffer->data[buffer->length] = '\0';
  return true;
}

static bool buffer_append(Buffer *buffer, const char *text) {
  return buffer_append_n(buffer, text, strlen(text));
}

static bool buffer_printf(Buffer *buffer, const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int needed = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (needed < 0 || !buffer_reserve(buffer, (size_t)needed)) {
    va_end(args);
    return false;
  }
  vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length,
            format, args);
  va_end(args);
  buffer->length += (size_t)needed;
  return true;
}

static bool buffer_json_string(Buffer *buffer, const char *text) {
  if (!buffer_append(buffer, "\""))
    return false;
  for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
    switch (*p) {
    case '"':
      if (!buffer_append(buffer, "\\\""))
        return false;
      break;
    case '\\':
      if (!buffer_append(buffer, "\\\\"))
        return false;
      break;
    case '\b':
      if (!buffer_append(buffer, "\\b"))
        return false;
      break;
    case '\f':
      if (!buffer_append(buffer, "\\f"))
        return false;
      break;
    case '\n':
      if (!buffer_append(buffer, "\\n"))
        return false;
      break;
    case '\r':
      if (!buffer_append(buffer, "\\r"))
        return false;
      break;
    case '\t':
      if (!buffer_append(buffer, "\\t"))
        return false;
      break;
    default:
      if (*p < 0x20) {
        if (!buffer_printf(buffer, "\\u%04x", *p))
          return false;
      } else if (!buffer_append_n(buffer, (const char *)p, 1)) {
        return false;
      }
    }
  }
  return buffer_append(buffer, "\"");
}

static void send_buffer(Buffer *buffer) {
  printf("Content-Length: %zu\r\n\r\n", buffer->length);
  fwrite(buffer->data, 1, buffer->length, stdout);
  fflush(stdout);
}

static void send_result(const char *id, const char *result) {
  Buffer response = {0};
  buffer_append(&response, "{\"jsonrpc\":\"2.0\",\"id\":");
  buffer_append(&response, id);
  buffer_append(&response, ",\"result\":");
  buffer_append(&response, result);
  buffer_append(&response, "}");
  send_buffer(&response);
  free(response.data);
}

static const char *json_value(const char *json, const char *key) {
  Buffer needle = {0};
  buffer_printf(&needle, "\"%s\"", key);
  const char *found = strstr(json, needle.data);
  free(needle.data);
  if (!found)
    return NULL;
  found = strchr(found, ':');
  if (!found)
    return NULL;
  found++;
  while (isspace((unsigned char)*found))
    found++;
  return found;
}

static void append_utf8(Buffer *buffer, uint32_t value) {
  char bytes[4];
  size_t count;
  if (value <= 0x7f) {
    bytes[0] = (char)value;
    count = 1;
  } else if (value <= 0x7ff) {
    bytes[0] = (char)(0xc0 | (value >> 6));
    bytes[1] = (char)(0x80 | (value & 0x3f));
    count = 2;
  } else {
    bytes[0] = (char)(0xe0 | (value >> 12));
    bytes[1] = (char)(0x80 | ((value >> 6) & 0x3f));
    bytes[2] = (char)(0x80 | (value & 0x3f));
    count = 3;
  }
  buffer_append_n(buffer, bytes, count);
}

static char *json_decode_string(const char *value) {
  if (!value || *value != '"')
    return NULL;
  Buffer decoded = {0};
  for (const char *p = value + 1; *p && *p != '"'; p++) {
    if (*p != '\\') {
      buffer_append_n(&decoded, p, 1);
      continue;
    }
    p++;
    if (!*p)
      break;
    switch (*p) {
    case '"':
    case '\\':
    case '/':
      buffer_append_n(&decoded, p, 1);
      break;
    case 'b':
      buffer_append_n(&decoded, "\b", 1);
      break;
    case 'f':
      buffer_append_n(&decoded, "\f", 1);
      break;
    case 'n':
      buffer_append_n(&decoded, "\n", 1);
      break;
    case 'r':
      buffer_append_n(&decoded, "\r", 1);
      break;
    case 't':
      buffer_append_n(&decoded, "\t", 1);
      break;
    case 'u': {
      uint32_t codepoint = 0;
      for (int i = 0; i < 4 && p[1]; i++) {
        p++;
        codepoint *= 16;
        if (*p >= '0' && *p <= '9')
          codepoint += (uint32_t)(*p - '0');
        else if (*p >= 'a' && *p <= 'f')
          codepoint += (uint32_t)(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F')
          codepoint += (uint32_t)(*p - 'A' + 10);
      }
      append_utf8(&decoded, codepoint);
      break;
    }
    default:
      buffer_append_n(&decoded, p, 1);
    }
  }
  if (!decoded.data) {
    decoded.data = malloc(1);
    if (decoded.data)
      decoded.data[0] = '\0';
  }
  return decoded.data;
}

static char *json_string(const char *json, const char *key) {
  return json_decode_string(json_value(json, key));
}

static char *json_id(const char *json) {
  const char *value = json_value(json, "id");
  if (!value)
    return NULL;
  if (*value == '"') {
    const char *p = value + 1;
    while (*p) {
      if (*p == '"' && p[-1] != '\\')
        break;
      p++;
    }
    size_t length = (size_t)(p - value + (*p == '"' ? 1 : 0));
    char *id = malloc(length + 1);
    if (!id)
      return NULL;
    memcpy(id, value, length);
    id[length] = '\0';
    return id;
  }
  const char *end = value;
  while (*end && (isdigit((unsigned char)*end) || *end == '-'))
    end++;
  if (end == value)
    return NULL;
  size_t length = (size_t)(end - value);
  char *id = malloc(length + 1);
  if (!id)
    return NULL;
  memcpy(id, value, length);
  id[length] = '\0';
  return id;
}

static int json_integer_after(const char *json, const char *anchor,
                              const char *key) {
  const char *start = anchor ? strstr(json, anchor) : json;
  if (!start)
    return 0;
  const char *value = json_value(start, key);
  return value ? atoi(value) : 0;
}

static Document *find_document(const char *uri) {
  for (Document *document = documents; document; document = document->next)
    if (strcmp(document->uri, uri) == 0)
      return document;
  return NULL;
}

static void set_document(const char *uri, char *text) {
  Document *document = find_document(uri);
  if (!document) {
    document = calloc(1, sizeof(*document));
    if (!document) {
      free(text);
      return;
    }
    document->uri = copy_string(uri);
    document->next = documents;
    documents = document;
  }
  free(document->text);
  document->text = text;
}

static void close_document(const char *uri) {
  Document **slot = &documents;
  while (*slot) {
    Document *document = *slot;
    if (strcmp(document->uri, uri) == 0) {
      *slot = document->next;
      free(document->uri);
      free(document->text);
      free(document);
      return;
    }
    slot = &document->next;
  }
}

static void collect_parser_diagnostic(void *context, const char *filename,
                                      uint32_t line, uint32_t column,
                                      const char *message) {
  (void)filename;
  Diagnostics *diagnostics = context;
  if (diagnostics->count == diagnostics->capacity) {
    size_t capacity = diagnostics->capacity ? diagnostics->capacity * 2 : 8;
    Diagnostic *grown = realloc(diagnostics->items,
                                capacity * sizeof(*diagnostics->items));
    if (!grown)
      return;
    diagnostics->items = grown;
    diagnostics->capacity = capacity;
  }
  diagnostics->items[diagnostics->count++] = (Diagnostic){
      .line = line, .column = column, .message = copy_string(message)};
}

static bool analysis_init(Analysis *analysis, const Document *document) {
  memset(analysis, 0, sizeof(*analysis));
  if (!arena_init(&analysis->arena))
    return false;
  strtab_init(&analysis->strings, &analysis->arena);
  lexer_init(&analysis->lexer, document->text, &analysis->strings);
  parser_init(&analysis->parser, &analysis->lexer, &analysis->arena,
              document->uri, document->text);
  parser_set_diagnostic_handler(&analysis->parser, collect_parser_diagnostic,
                                &analysis->diagnostics);
  analysis->program = parser_parse(&analysis->parser);
  return true;
}

static void analysis_destroy(Analysis *analysis) {
  for (size_t i = 0; i < analysis->diagnostics.count; i++)
    free(analysis->diagnostics.items[i].message);
  free(analysis->diagnostics.items);
  arena_destroy(&analysis->arena);
}

static void append_range(Buffer *buffer, uint32_t line, uint32_t column,
                         size_t width) {
  uint32_t lsp_line = line ? line - 1 : 0;
  uint32_t lsp_column = column ? column - 1 : 0;
  buffer_printf(buffer,
                "{\"start\":{\"line\":%u,\"character\":%u},"
                "\"end\":{\"line\":%u,\"character\":%zu}}",
                lsp_line, lsp_column, lsp_line, (size_t)lsp_column + width);
}

static void publish_diagnostics(const Document *document, Analysis *analysis) {
  Buffer response = {0};
  buffer_append(
      &response,
      "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
      "\"params\":{\"uri\":");
  buffer_json_string(&response, document->uri);
  buffer_append(&response, ",\"diagnostics\":[");
  for (size_t i = 0; i < analysis->diagnostics.count; i++) {
    Diagnostic *diagnostic = &analysis->diagnostics.items[i];
    if (i)
      buffer_append(&response, ",");
    buffer_append(&response, "{\"range\":");
    append_range(&response, diagnostic->line, diagnostic->column, 1);
    buffer_append(&response, ",\"severity\":1,\"source\":\"runes\","
                             "\"message\":");
    buffer_json_string(&response, diagnostic->message);
    buffer_append(&response, "}");
  }
  buffer_append(&response, "]}}");
  send_buffer(&response);
  free(response.data);
}

static void analyze_and_publish(const Document *document) {
  Analysis analysis;
  if (!analysis_init(&analysis, document))
    return;
  publish_diagnostics(document, &analysis);
  analysis_destroy(&analysis);
}

static const char *node_name(AstNode *node) {
  switch (node->kind) {
  case AST_FUNC_DECL:
    return node->as.func_decl.name;
  case AST_VAR_DECL:
    return node->as.var_decl.name;
  case AST_TYPE_DECL:
    return node->as.type_decl.name;
  case AST_VARIANT_DECL:
    return node->as.variant_decl.name;
  case AST_VARIANT_ARM:
    return node->as.variant_arm.name;
  case AST_FIELD_DECL:
    return node->as.field_decl.name;
  case AST_INTERFACE_DECL:
    return node->as.interface_decl.name;
  case AST_ERROR_DECL:
    return node->as.error_decl.name;
  case AST_MOD_DECL:
    return node->as.mod_decl.name;
  case AST_EXTERN_DECL:
    return node->as.extern_decl.name;
  case AST_PARAM:
    return node->as.param.name;
  default:
    return NULL;
  }
}

static int node_symbol_kind(AstNode *node) {
  switch (node->kind) {
  case AST_FUNC_DECL:
    return 12;
  case AST_VAR_DECL:
    return node->as.var_decl.is_const ? 14 : 13;
  case AST_TYPE_DECL:
    return 23;
  case AST_VARIANT_DECL:
  case AST_ERROR_DECL:
    return 10;
  case AST_VARIANT_ARM:
    return 22;
  case AST_FIELD_DECL:
    return 8;
  case AST_INTERFACE_DECL:
    return 11;
  case AST_MOD_DECL:
    return 2;
  case AST_EXTERN_DECL:
    return node->as.extern_decl.is_func ? 12 : 13;
  case AST_PARAM:
    return 13;
  default:
    return 0;
  }
}

static AstNode *node_children(AstNode *node) {
  switch (node->kind) {
  case AST_FUNC_DECL:
    return node->as.func_decl.params;
  case AST_TYPE_DECL:
    return node->as.type_decl.fields;
  case AST_VARIANT_DECL:
    return node->as.variant_decl.arms;
  case AST_INTERFACE_DECL:
    return node->as.interface_decl.methods;
  case AST_ERROR_DECL:
    return node->as.error_decl.variants;
  case AST_MOD_DECL:
    return node->as.mod_decl.declarations;
  case AST_METHOD_DECL:
    return node->as.method_decl.methods;
  case AST_EXTERN_DECL:
    return node->as.extern_decl.params;
  default:
    return NULL;
  }
}

static void append_symbol_list(Buffer *buffer, AstNode *node);

static void append_symbol(Buffer *buffer, AstNode *node) {
  const char *name = node_name(node);
  int kind = node_symbol_kind(node);
  if (!name || !kind)
    return;
  buffer_append(buffer, "{\"name\":");
  buffer_json_string(buffer, name);
  buffer_printf(buffer, ",\"kind\":%d,\"range\":", kind);
  append_range(buffer, node->line, node->col, strlen(name));
  buffer_append(buffer, ",\"selectionRange\":");
  append_range(buffer, node->line, node->col, strlen(name));
  AstNode *children = node_children(node);
  if (children) {
    buffer_append(buffer, ",\"children\":[");
    append_symbol_list(buffer, children);
    buffer_append(buffer, "]");
  }
  buffer_append(buffer, "}");
}

static void append_symbol_list(Buffer *buffer, AstNode *node) {
  bool first = true;
  for (; node; node = node->next) {
    if (node->kind == AST_METHOD_DECL) {
      AstNode *methods = node->as.method_decl.methods;
      for (; methods; methods = methods->next) {
        if (!first)
          buffer_append(buffer, ",");
        append_symbol(buffer, methods);
        first = false;
      }
      continue;
    }
    if (!node_name(node) || !node_symbol_kind(node))
      continue;
    if (!first)
      buffer_append(buffer, ",");
    append_symbol(buffer, node);
    first = false;
  }
}

static void handle_document_symbols(const char *id, const Document *document) {
  Analysis analysis;
  if (!analysis_init(&analysis, document)) {
    send_result(id, "[]");
    return;
  }
  Buffer result = {0};
  buffer_append(&result, "[");
  if (analysis.program)
    append_symbol_list(&result, analysis.program->as.program.declarations);
  buffer_append(&result, "]");
  send_result(id, result.data);
  free(result.data);
  analysis_destroy(&analysis);
}

static size_t utf8_sequence_length(unsigned char byte) {
  if ((byte & 0x80) == 0)
    return 1;
  if ((byte & 0xe0) == 0xc0)
    return 2;
  if ((byte & 0xf0) == 0xe0)
    return 3;
  if ((byte & 0xf8) == 0xf0)
    return 4;
  return 1;
}

static const char *position_pointer(const char *text, int target_line,
                                    int target_character) {
  const char *p = text;
  for (int line = 0; line < target_line && *p; line++) {
    const char *newline = strchr(p, '\n');
    if (!newline)
      return p + strlen(p);
    p = newline + 1;
  }
  int utf16 = 0;
  while (*p && *p != '\n' && utf16 < target_character) {
    size_t length = utf8_sequence_length((unsigned char)*p);
    utf16 += length == 4 ? 2 : 1;
    p += length;
  }
  return p;
}

static char *word_at_position(const char *text, int line, int character) {
  const char *position = position_pointer(text, line, character);
  const char *start = position;
  while (start > text &&
         (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
    start--;
  const char *end = position;
  while (isalnum((unsigned char)*end) || *end == '_')
    end++;
  if (start == end)
    return NULL;
  size_t length = (size_t)(end - start);
  char *word = malloc(length + 1);
  if (!word)
    return NULL;
  memcpy(word, start, length);
  word[length] = '\0';
  return word;
}

static AstNode *find_declaration(AstNode *node, const char *name) {
  for (; node; node = node->next) {
    const char *candidate = node_name(node);
    if (candidate && strcmp(candidate, name) == 0)
      return node;
    AstNode *found = find_declaration(node_children(node), name);
    if (found)
      return found;
  }
  return NULL;
}

static const char *node_kind_name(AstNode *node) {
  switch (node->kind) {
  case AST_FUNC_DECL:
    return "function";
  case AST_VAR_DECL:
    return node->as.var_decl.is_const ? "constant" : "variable";
  case AST_TYPE_DECL:
    return "type";
  case AST_VARIANT_DECL:
    return "variant";
  case AST_VARIANT_ARM:
    return "variant arm";
  case AST_FIELD_DECL:
    return "field";
  case AST_INTERFACE_DECL:
    return "interface";
  case AST_ERROR_DECL:
    return "error set";
  case AST_MOD_DECL:
    return "module";
  case AST_EXTERN_DECL:
    return node->as.extern_decl.is_func ? "extern function" : "extern variable";
  case AST_PARAM:
    return "parameter";
  default:
    return "declaration";
  }
}

static AstNode *declaration_at_request(const char *json,
                                       const Document *document,
                                       Analysis *analysis, char **word) {
  int line = json_integer_after(json, "\"position\"", "line");
  int character = json_integer_after(json, "\"position\"", "character");
  *word = word_at_position(document->text, line, character);
  if (!*word || !analysis_init(analysis, document))
    return NULL;
  return analysis->program
             ? find_declaration(analysis->program->as.program.declarations,
                                *word)
             : NULL;
}

static void handle_hover(const char *id, const char *json,
                         const Document *document) {
  Analysis analysis;
  char *word = NULL;
  AstNode *declaration =
      declaration_at_request(json, document, &analysis, &word);
  if (!declaration) {
    if (word)
      analysis_destroy(&analysis);
    free(word);
    send_result(id, "null");
    return;
  }
  Buffer result = {0};
  buffer_append(&result, "{\"contents\":{\"kind\":\"markdown\",\"value\":");
  Buffer description = {0};
  buffer_printf(&description, "`%s` — Runes %s", word,
                node_kind_name(declaration));
  buffer_json_string(&result, description.data);
  buffer_append(&result, "}}");
  send_result(id, result.data);
  free(description.data);
  free(result.data);
  free(word);
  analysis_destroy(&analysis);
}

static void handle_definition(const char *id, const char *json,
                              const Document *document) {
  Analysis analysis;
  char *word = NULL;
  AstNode *declaration =
      declaration_at_request(json, document, &analysis, &word);
  if (!declaration) {
    if (word)
      analysis_destroy(&analysis);
    free(word);
    send_result(id, "null");
    return;
  }
  Buffer result = {0};
  buffer_append(&result, "{\"uri\":");
  buffer_json_string(&result, document->uri);
  buffer_append(&result, ",\"range\":");
  append_range(&result, declaration->line, declaration->col, strlen(word));
  buffer_append(&result, "}");
  send_result(id, result.data);
  free(result.data);
  free(word);
  analysis_destroy(&analysis);
}

static void handle_completion(const char *id) {
  static const char *items[] = {
      "f",       "type",    "interface", "method",   "error",  "mod",
      "use",     "pub",     "const",     "extern",   "if",     "else",
      "when",    "realm",    "in",      "except",
      "while",   "for",     "loop",      "match",    "return", "break",
      "continue","try",     "catch",     "unsafe",   "asm",    "promote",
      "stack",   "regional","dynamic",   "gc",       "flex",   "i8",
      "i16",     "i32",     "i64",       "u8",       "u16",    "u32",
      "u64",     "f32",     "f64",       "bool",     "str",    "char",
      "usize",   "void",    "true",      "false",    "null",
  };
  Buffer result = {0};
  buffer_append(&result, "[");
  for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
    if (i)
      buffer_append(&result, ",");
    buffer_append(&result, "{\"label\":");
    buffer_json_string(&result, items[i]);
    buffer_append(&result, ",\"kind\":14}");
  }
  buffer_append(&result, "]");
  send_result(id, result.data);
  free(result.data);
}

static char *read_message(void) {
  char header[1024];
  size_t content_length = 0;
  while (fgets(header, sizeof(header), stdin)) {
    if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0)
      break;
    if (strncmp(header, "Content-Length:", 15) == 0)
      content_length = (size_t)strtoull(header + 15, NULL, 10);
  }
  if (!content_length)
    return NULL;
  char *message = malloc(content_length + 1);
  if (!message)
    return NULL;
  if (fread(message, 1, content_length, stdin) != content_length) {
    free(message);
    return NULL;
  }
  message[content_length] = '\0';
  return message;
}

static void clear_diagnostics(const char *uri) {
  Buffer response = {0};
  buffer_append(
      &response,
      "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
      "\"params\":{\"uri\":");
  buffer_json_string(&response, uri);
  buffer_append(&response, ",\"diagnostics\":[]}}");
  send_buffer(&response);
  free(response.data);
}

int main(void) {
  bool shutdown = false;
  while (true) {
    char *message = read_message();
    if (!message)
      break;
    char *method = json_string(message, "method");
    char *id = json_id(message);

    if (method && strcmp(method, "initialize") == 0 && id) {
      send_result(
          id,
          "{\"capabilities\":{\"textDocumentSync\":1,"
          "\"documentSymbolProvider\":true,\"hoverProvider\":true,"
          "\"definitionProvider\":true,"
          "\"completionProvider\":{\"triggerCharacters\":[\".\"]}},"
          "\"serverInfo\":{\"name\":\"runes-lsp\",\"version\":\"0.1.0\"}}");
    } else if (method && strcmp(method, "shutdown") == 0 && id) {
      shutdown = true;
      send_result(id, "null");
    } else if (method && strcmp(method, "exit") == 0) {
      free(method);
      free(id);
      free(message);
      break;
    } else if (method &&
               (strcmp(method, "textDocument/didOpen") == 0 ||
                strcmp(method, "textDocument/didChange") == 0)) {
      char *uri = json_string(message, "uri");
      const char *text_scope =
          strcmp(method, "textDocument/didChange") == 0
              ? strstr(message, "\"contentChanges\"")
              : message;
      char *text = text_scope ? json_string(text_scope, "text") : NULL;
      if (uri && text) {
        set_document(uri, text);
        Document *document = find_document(uri);
        if (document)
          analyze_and_publish(document);
      } else {
        free(text);
      }
      free(uri);
    } else if (method && strcmp(method, "textDocument/didClose") == 0) {
      char *uri = json_string(message, "uri");
      if (uri) {
        clear_diagnostics(uri);
        close_document(uri);
      }
      free(uri);
    } else if (method && id) {
      char *uri = json_string(message, "uri");
      Document *document = uri ? find_document(uri) : NULL;
      if (strcmp(method, "textDocument/documentSymbol") == 0 && document)
        handle_document_symbols(id, document);
      else if (strcmp(method, "textDocument/hover") == 0 && document)
        handle_hover(id, message, document);
      else if (strcmp(method, "textDocument/definition") == 0 && document)
        handle_definition(id, message, document);
      else if (strcmp(method, "textDocument/completion") == 0)
        handle_completion(id);
      else
        send_result(id, "null");
      free(uri);
    }

    free(method);
    free(id);
    free(message);
  }

  while (documents)
    close_document(documents->uri);
  return shutdown ? 0 : 1;
}
