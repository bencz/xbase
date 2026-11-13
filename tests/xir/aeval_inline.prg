// AEval of an array literal with a codeblock literal: the runtime's AEval (a carried source) runs in place, ValType()
// of the array and of the block fold, so only the array's path stays: a counted loop whose body is the block's,
// inlined too; the block is no longer made and the variable it captured lives in a register.
FUNCTION Main()
   LOCAL n := 0
   AEval( { 1, 2, 3 }, {| x | n += x } )
   RETURN n
