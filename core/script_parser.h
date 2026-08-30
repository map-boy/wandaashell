#pragma once
#include <vector>
#include <string>
#include "core/ast.h"

// Parses raw .waa source text into a Program AST node.
// Grammar covered by this first pass:
//   assign:   ident = expr
//   if:       if expr { ... } else { ... }
//   loop:     loop expr { ... }
//   call:     ident ( arg, arg, ... )
//   expr:     literal | ident | expr op expr | call
NodePtr parseScript(const std::string& source);