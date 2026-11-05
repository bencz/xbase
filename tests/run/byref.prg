// Arguments passed by reference (@): LOCALs, STATICs, parameters passed on, codeblocks sharing the variable,
// Eval passing references to the block, array elements and FOR EACH variables.
STATIC s := 1
PROCEDURE Main()
   LOCAL x := 1, a := { 1, 2 }, b, e, y
   SetTo( @x, 5 )
   ? "local", x
   SetTo( @s, 7 )
   ? "static", s
   Pass( @x )
   ? "passed on", x
   b := Capture( @x )
   x := 100
   ? "block sees", Eval( b )
   Eval( b, 200 )
   ? "block wrote", x
   Eval( {| p | p := 33 }, @x )
   ? "eval byref", x
   SetTo( @a[ 2 ], 9 )
   ? "element", a[ 1 ], a[ 2 ]
   FOR EACH e IN a
      SetTo( @e, 50 )
   NEXT
   ? "foreach", a[ 1 ], a[ 2 ]
   x := 1
   SetTo( x, 5 )
   ? "by value", x
   x := 1
   ? "read param", Twice( @x ), x
   x := 1
   y := Keep( @x )
   x := 2
   ? "copy of param", Eval( y )
   x := { 1, 2 }
   SetTo( @x, 3 )
   ? "array var", x
   x := 1
   ? "valtype", VType( @x )
   x := 10
   ? "nested", Outer( @x ), x
   ? "block param", Eval( {| p | SetTo( @p, 4 ), p }, 1 )
   x := "abc"
   ? "native by value", Len( @x ), x
   x := 0
   FOR y := 1 TO 20000
      Add( @x, Replicate( "x", y % 7 ) )
   NEXT
   ? "many calls", x
   a := { 1, 2, 3 }
   FOR EACH s IN a
      SetTo( @s, s * 100 )
   NEXT
   ? "static in FOR EACH", a[ 1 ], a[ 2 ], a[ 3 ], s
   RETURN

PROCEDURE Add( n, c )
   n := n + Len( c )
   RETURN

PROCEDURE SetTo( v, n )
   v := n
   RETURN

PROCEDURE Pass( v )
   SetTo( @v, 6 )
   RETURN

FUNCTION Capture( v )
   RETURN {| n | IIf( n == NIL, v, v := n ) }

FUNCTION Twice( v )
   v := v * 2
   RETURN v + 1

FUNCTION Keep( v )
   LOCAL c := v
   RETURN {|| c }

FUNCTION VType( v )
   RETURN ValType( v )

FUNCTION Outer( v )
   LOCAL r := Inner( @v )
   RETURN r + v

FUNCTION Inner( w )
   w += 5
   RETURN w
