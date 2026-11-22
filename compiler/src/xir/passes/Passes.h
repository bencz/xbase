#pragma once

#include "xir/IR.h"

#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace xb::xir {

// The optimization passes on xIR (docs/xir.md, Passes). Each one transforms a
// function in place and returns whether it changed anything.

// A STATIC FUNCTION whose calls in the module pass no reference never runs its body versioned for references
// (`NAME.byref`): its versioning test becomes false and that body leaves the module (Versioning.cpp).
bool dropReferenceBodies(Module &m);

// When no call of `f` passes a reference: its versioning test becomes false and the branch to its body versioned for
// references goes; that body, which only the test reached, is returned (null without a test) for the caller to remove
// (Versioning.cpp).
Function *dropReferenceTest(Function &f);

// A call of a small function of the module, or of a function of the runtime's carried sources (Module::library), runs
// its body in place, shown on the call stack as the function (inline.enter, inline.leave), its variables the caller's
// own (Inline.cpp).
bool inlineFunctions(Module &m, Function &f);

// `Eval( {| p | body }, x )` of a codeblock literal of the function: the body runs in place, shown on the call stack as
// the block, its variables the function's own (Inline.cpp).
bool inlineBlocks(Function &f);

// A frame variable that nothing captures or passes by reference any more holds no cell (`shared`): it is read and
// written directly (no barrier: it is never in the heap), which lets promote-variables take it (Inline.cpp).
bool unshareVariables(Function &f);

// A send to an object whose class is known exactly (one of the module's, its whole line the module's) is what the
// runtime would do: a call of the method it reaches, or an object.get or object.set of the variable (ResolveSends.cpp).
bool resolveSends(Module &m, Function &f);
// A send to an object whose class is not known, versioned on its class when the classes of the module that answer it
// all reach one method (or one class an instance variable): resolved for those, the send kept for the others.
bool speculateSends(Module &m, Function &f);

// Sends to values of a built-in type whose class of the runtime answers them, and chains of Enumerable's methods on
// what they make, become calls of those methods behind a test that no class of the program answers first
// (ScalarSends.cpp).
bool resolveScalarSends(Module &m, Function &f);

// `x := x + y` on a LOCAL only the function sees and that holds strings
// becomes one `append`, whose string grows in place (AppendInPlace.cpp).
bool appendInPlace(Function &f);
bool splitAppends(Function &f);

// mem2reg: LOCALs reached only by direct loads and stores (not shared, not
// parameters, not FOR EACH variables) become SSA values, with block
// parameters where their definitions meet; they leave the frame.
bool promoteVariables(Function &f);

// Operations on constant operands become constants; a branch on a constant
// condition becomes a jump.
bool foldConstants(Function &f);

// A STATIC its initializer alone writes, with a constant (not passed by reference nor a FOR EACH variable, assigned
// nowhere else in the module), holds that constant wherever code reads it: every module's STATICs are initialized,
// with constants, before any code of the program runs (xb_program_run). Each of its loads becomes the constant
// (FoldConstants.cpp).
bool foldConstantStatics(Module &m);

// Removes unreachable blocks, sends branches past blocks that only jump on,
// merges a block into its single predecessor when that one jumps to it, and
// turns two branches to one block that differ in a true/false argument into a
// jump passing the condition.
bool simplifyCFG(Function &f);

// Removes instructions whose result is unused and that do nothing else (see
// Effect), and block parameters nothing uses.
bool removeDeadCode(Function &f);

// Checks the result of an operation whose error may substitute it, where the
// types it gives itself hold no heap reference: a substituted value of another
// type continues in a generic copy of the rest of the function
// (TypedContinuations.cpp).
bool typedContinuations(Function &f);

// The typed copies of the module's functions made so far, by function and their parameters' types then classes
// (Specialize.cpp).
using TypedCopies = std::map<std::pair<const Function *, std::vector<uint32_t>>, Function *>;

// A call of a function of the module whose arguments hold numbers, logicals or dates goes to a copy of the function
// whose parameters have those types (Function::paramTypes), made once and optimized by `optimizeCopy`
// (Specialize.cpp).
bool specializeCalls(Module &m, Function &f, TypedCopies &copies, const std::function<void(Function &)> &optimizeCopy);

// Once every function is optimized, the calls between typed copies move to the copies the types of each other's
// results ask for: one optimistic fixed point over every copy at once, which their own settling, one at a time, cannot
// reach where they call each other (a result passed to a call, `Ack( m - 1, Ack( m, n - 1 ) )`); kept only when no
// call then gives types beyond those the code around it was optimized for (Specialize.cpp).
bool refineCopies(Module &m, TypedCopies &copies);

// A STATIC FUNCTION that only its typed copies stand for now (nothing calls or names it) leaves the module, with its
// body versioned for references, and so does a copy nothing reaches any more (Specialize.cpp).
bool dropCopiedFunctions(Module &m, const TypedCopies &copies);

// The return types of a typed copy whose calls of itself give what it returns: the least fixed point (Specialize.cpp).
void settleReturnTypes(Function &f);

// Peels the first pass of a small innermost loop that carries a value whose type the first pass changes (a sum that
// starts at the integer 0 and adds doubles): the loop proper then carries it with one type (PeelTypes.cpp).
bool peelForTypes(Function &f);

// Runs a generator of the module in place in the FOR EACH that goes through the enumerator made right there: its body
// copied into the function, each YIELD entering the loop's body (FuseGenerators.cpp).
bool fuseGenerators(Function &f);

// Versions FOR EACH loops whose collection may be a string, an array or a hash, among other things: a test of its type
// before the loop enters a copy made for that type, where the collection's type is known (VersionEach.cpp).
bool versionEachLoops(Function &f);

// A generator's values that live across a YIELD move into frame variables, which the enumerator saves between steps
// (Resumable.cpp). Not an optimization: Gen runs it on every generator, and the other passes leave such functions'
// variables in memory.
bool makeResumable(Function &f);

} // namespace xb::xir
