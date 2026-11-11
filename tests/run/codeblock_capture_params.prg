// Codeblocks capturing the parameters of the codeblocks around them (which the reference rejects as "out of reach"):
// a block that makes another one closing over its parameter, a counter kept in a parameter's cell, a parameter
// changed by an inner block, one passed by reference whose inner block writes the caller's variable, and two
// parameters captured by a block passed to AEval; also the plain case Clipper rejects, Eval in Eval.
PROCEDURE Main()
   LOCAL mk := {| k | {|| k * 2 } }, add := {| n | {| x | x + n } }, b, c, v := 1, cnt := {| n | {|| ++n } }
   ? Eval( Eval( mk, 21 ) ), Eval( Eval( add, 10 ), 5 )
   b := Eval( cnt, 0 )
   ? Eval( b ), Eval( b ), Eval( b )
   c := {| x | Eval( {|| x := x + 1 } ), x }
   ? Eval( c, 5 )
   b := {| r | Eval( {|| r := "changed" } ) }
   Eval( b, @v )
   ? v
   ? Eval( {| a, b | AEval( { 1, 2 }, {| x | a += x * b } ), a }, 100, 10 )
   ? Eval( {| x | Eval( {| y | x * y }, 3 ) }, 2 )
RETURN
