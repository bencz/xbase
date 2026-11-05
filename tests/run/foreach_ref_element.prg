// A parameter that received an array element by reference is shared: FOR EACH over it works inside it, codeblocks
// taken during the loop see the current element and, after it, the element of the caller.
PROCEDURE Main()
   LOCAL arr := { "e1" }
   P( @arr[ 1 ] )
   ? "arr after", arr[ 1 ]
   RETURN

PROCEDURE P( q )
   LOCAL bl, c := {}
   FOR EACH q IN { "a", "b" }
      bl := {| n | IIf( n == NIL, q, q := n ) }
      AAdd( c, bl )
      ?? " during", Eval( bl ), Eval( c[ 1 ] )
   NEXT
   ? "after", q, Eval( bl ), Eval( c[ 1 ] )
   Eval( bl, "w" )
   ? "wrote", q
   RETURN
