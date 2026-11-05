// The preprocessor's limits, in one place, and the token helpers its files use.
#pragma once

#include "common/Token.h"

#include <cstddef>

namespace xb::pp {

constexpr int kMaxIncludeDepth = 64;
constexpr int kMaxStatementDepth = 64;
// Limits shared by everything one source statement expands into, so that
// circular or self-multiplying rules stop quickly instead of exhausting memory.
constexpr int kRewriteBudget = 4096;
constexpr size_t kMaxStatementTokens = 65536;

constexpr int kMaxOptionalNesting = 64;     // [ ] nesting inside one rule
constexpr int kMaxConditionDepth = 256;     // #if expression nesting

inline bool isOpen(Tok k) { return k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace; }
inline bool isClose(Tok k) { return k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace; }
inline Tok closerOf(Tok k) { return k == Tok::LParen ? Tok::RParen : k == Tok::LBracket ? Tok::RBracket : Tok::RBrace; }

} // namespace xb::pp
