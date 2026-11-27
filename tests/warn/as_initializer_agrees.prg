// Initializers of the declared types give no warning: a number, a string, NIL (no value yet), any value (USUAL), one
// of alternatives, an expression (not a literal), an array literal AS ARRAY OF a type, a codeblock AS CODEBLOCK.
PROCEDURE Main()
   LOCAL n AS NUMERIC := 1, c AS STRING := "s", z AS NUMERIC := NIL, u AS USUAL := 1, y AS NUMERIC|STRING := "y"
   LOCAL e AS NUMERIC := Len( "abc" ), a AS ARRAY OF NUMERIC := { 1 }, b AS CODEBLOCK := {|| 1 }, i AS INTEGER := 2
   ? n, c, z, u, y, e, Len( a ), Eval( b ), i
RETURN
