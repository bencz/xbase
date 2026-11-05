// FOR EACH over a STATIC passed by reference inside the loop, over a parameter passed by reference, and over variables
// whose cells codeblocks hold.
STATIC s := 1, t := 1
PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, b := {}, x := "orig", c, a2 := { 10, 20 }, y := 1
   FOR EACH s IN a
      AAdd( b, Cap( @s ) )
      IF Len( b ) == 2
         ? "during", Eval( b[ 1 ] )
      ENDIF
   NEXT
   s := 42
   ? "after", Eval( b[ 1 ] ), Eval( b[ 3 ] )
   Eval( b[ 1 ], 99 )
   ? "wrote", s, a[ 1 ], a[ 2 ], a[ 3 ]
   c := {|| x }
   EachP( @x, a2, c )
   ? "after EachP", x
   b := Cap( @t )
   FOR EACH t IN a2
      ?? " static shared", Eval( b )
   NEXT
   c := Cap( @y )
   FOR EACH y IN a2
      ?? " local shared", Eval( c )
   NEXT
   ? "restored", t, y, Eval( b ), Eval( c )
   RETURN

PROCEDURE EachP( v, a, b )
   LOCAL k := {|| v }
   FOR EACH v IN a
      ?? " seen:", Eval( b ), Eval( k )
   NEXT
   RETURN

FUNCTION Cap( v )
   RETURN {| n | IIf( n == NIL, v, v := n ) }
