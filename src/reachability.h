#ifndef RUNES_REACHABILITY_H
#define RUNES_REACHABILITY_H

#include "ast.h"

/* Mark declarations needed by executable code after semantic analysis. */
void reachability_mark(AstNode *program);

#endif
