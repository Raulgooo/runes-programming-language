(block_comment) @comment
(comment) @comment
(string) @string
(character) @string
(attribute) @attribute
(float) @number
(integer) @number
(builtin_type) @type.builtin
(boolean) @boolean
(null) @constant.builtin
(special_constant) @constant.builtin
(keyword) @keyword
(identifier) @variable
(operator) @operator
(punctuation) @punctuation
["(" ")" "[" "]" "{" "}"] @punctuation.bracket

((keyword) @_function_keyword
  (identifier) @function
  (#eq? @_function_keyword "f"))

((keyword) @_type_keyword
  (identifier) @type
  (#any-of? @_type_keyword "type" "interface" "error"))

((keyword) @_module_keyword
  (identifier) @module
  (#eq? @_module_keyword "mod"))

((identifier) @function.call
  (parenthesized))

((identifier) @type
  (#match? @type "^[A-Z]"))
