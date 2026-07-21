#ifndef RUNES_MONOMORPHIZE_H
#define RUNES_MONOMORPHIZE_H

#include "ast.h"
#include "utils/arena.h"
#include <stdbool.h>

bool monomorphize_program(Arena *arena, AstNode *program);

#endif
