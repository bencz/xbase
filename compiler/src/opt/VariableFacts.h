#pragma once

#include "parser/AST.h"

#include <vector>

namespace xb {

// What a function does with each of its frame variables (by slot; a slot past a vector's size has the fact false),
// for the AST passes: assigned after its declaration (an assignment, ++/--, a reference @x, a FOR or FOR EACH variable,
// RECOVER USING), captured by one of its codeblocks, and used other than as a table: anything but the base of an
// element read (t[i], not assigned nor passed by reference) or Len( t ) (opt/LocalTables.cpp). A codeblock's use of a
// variable is a capture.
struct VariableFacts {
  std::vector<bool> written, captured, otherUse;
  static bool has(const std::vector<bool> &set, uint32_t slot) { return slot < set.size() && set[slot]; }
};

VariableFacts variableFacts(FuncDecl &f);

} // namespace xb
