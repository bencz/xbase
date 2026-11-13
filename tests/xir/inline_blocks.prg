// A codeblock literal that Eval runs where it is made: its body inlined between inline.enter and inline.leave, its
// parameter a variable of the function, and the variable only it captured no longer shared (no barrier after its
// stores). A block passed a reference (@n) keeps its parameter read through the reference, and a FOR EACH variable
// keeps its barrier. AEval's block is called.
FUNCTION Main( aList )
   LOCAL n := 0, m := 1, e
   AEval( aList, { | x | n += x } )
   m := Eval( { | k | k * m + 1 }, 2 )
   Eval( { | r | r++ }, @n )
   FOR EACH e IN aList
      e := Eval( { | v | v * 2 }, e )
   NEXT
   RETURN n + m
