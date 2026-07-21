/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "runes",

  extras: ($) => [/\s/],

  rules: {
    source_file: ($) => repeat($._token),

    _token: ($) =>
      choice(
        $.block_comment,
        $.comment,
        $.string,
        $.character,
        $.attribute,
        $.float,
        $.integer,
        $.builtin_type,
        $.boolean,
        $.null,
        $.special_constant,
        $.keyword,
        $.identifier,
        $.operator,
        $.punctuation,
      ),

    block_comment: (_) =>
      token(prec(3, seq("---", repeat(choice(/[^-]/, /-[^-]/, /--[^-]/)), "---"))),
    comment: (_) => token(prec(2, seq("--", /[^\n]*/))),

    string: (_) => token(seq('"', repeat(choice(/[^"\\\n]/, /\\./)), '"')),
    character: (_) => token(seq("'", choice(/[^'\\\n]/, /\\./), "'")),
    attribute: (_) => token(seq("#[", /[^\]\n]*/, "]")),

    float: (_) => token(/([0-9][0-9_]*)?\.[0-9][0-9_]*([eE][+-]?[0-9][0-9_]*)?/),
    integer: (_) => token(choice(/0x[0-9a-fA-F_]+/, /0b[01_]+/, /[0-9][0-9_]*/)),

    builtin_type: (_) =>
      choice(
        "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "f32", "f64", "bool", "str", "char", "usize", "void",
      ),
    boolean: (_) => choice("true", "false"),
    special_constant: (_) => choice("self", "Ok", "Err", "Some", "None"),
    keyword: (_) =>
      choice(
        "f", "type", "error", "mod", "use", "pub", "const", "method",
        "interface", "extern", "volatile", "flex", "stack", "dynamic",
        "regional", "gc", "if", "else", "while", "loop", "break",
        "continue", "return", "match", "for", "try", "catch", "unsafe",
        "asm", "promote", "move", "sizeof", "alignof", "as", "and", "or",
      ),
    null: (_) => "null",
    identifier: (_) => /[A-Za-z_][A-Za-z0-9_]*/,
    operator: (_) =>
      choice("..=", "..", "->", "<<", ">>", "==", "!=", "<=", ">=", ":=",
             "+", "-", "*", "/", "%", "=", "<", ">", "!", "?", "&", "|",
             "^", "~"),
    punctuation: (_) => choice("(", ")", "[", "]", "{", "}", ",", ":", ".", ";"),
  },
});
