#pragma once

#include "parser/AST.h"

#include <string>

namespace xb {

// The optimization passes on the syntax tree, after Sema (docs/compiler.md, AST passes): each one rewrites the tree or
// records facts on it for the code generator, and the verifier checks the tree after each one that changed it. Returns
// the verifier's report of the first pass that left the tree invalid ("" when every one left it valid).
std::string optimizeAst(Module &m);

// local-tables (LocalTables.cpp): LocalDecl::table.
bool markLocalTables(Module &m);

// value-captures (ValueCaptures.cpp): FuncDecl::capturedByValue.
bool markValueCaptures(Module &m);

// The checks of a resolved syntax tree: every variable reaches a slot of its frame, a capture of its codeblock or a
// STATIC of the module; the facts recorded have the frames' sizes. "" when it holds, else what is wrong.
std::string verifyAst(const Module &m);

// The syntax tree as text (--dump-ast): each node on a line of its own, its children indented below it.
std::string printAst(const Module &m);

} // namespace xb
