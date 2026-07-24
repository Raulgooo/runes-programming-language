; Zed's outline panel is populated from Tree-sitter captures, independently
; from the LSP's textDocument/documentSymbol response.
;
; The current Runes grammar deliberately keeps declarations structurally
; lightweight, so the declaration name is both the navigable item and its
; displayed name. The preceding keyword supplies useful context in the panel.

; Functions, including functions nested inside method blocks.
((keyword) @context
  .
  (identifier) @name @item
  (#eq? @context "f"))

; Named top-level declarations and method blocks.
((keyword) @context
  .
  (identifier) @name @item
  (#any-of? @context
    "type"
    "interface"
    "error"
    "method"
    "mod"))
