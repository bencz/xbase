#pragma once

#include "common/Diagnostics.h"
#include "parser/AST.h"

namespace xb {

// Makes a module's classes functions, before the other passes: each method's
// implementation (METHOD, ACCESS or ASSIGN name ... CLASS cls) becomes the
// STATIC function "CLS:MESSAGE" whose first parameter is SELF, an INLINE
// method a function returning its expression, the class gets its class
// function NAME() (a new object) and, when its variables have INIT values, a
// function returning them as an array. It reports a class defined twice, a
// member declared twice, an implementation of nothing declared or twice, and
// a declared method without one.
class ClassResolver {
public:
  explicit ClassResolver(Diagnostics &diag) : diag_(diag) {}
  void run(Module &m);

private:
  Diagnostics &diag_;
};

} // namespace xb
