// IIF results passed as call arguments and array elements, nested, with
// branches that call functions and build arrays.
PROCEDURE Main()
   LOCAL a := 1, b := 2, i, x
   ? Pair( a, IIf( a > b, "gt", "le" ) )
   ? Pair( IIf( a < b, Pair( "x", IIf( b > 1, b * 10, -1 ) ), "none" ), IIf( .F., 1, a + b ) )
   x := { IIf( a == 1, "one", "other" ), IIf( b == 1, "one", { b, IIf( a > 0, a, 0 ) } ), a }
   ? Len( x ), x[ 1 ], ValType( x[ 2 ] ), x[ 2 ][ 1 ], x[ 2 ][ 2 ], x[ 3 ]
   FOR i := 1 TO 4
      ? i, Pair( IIf( i % 2 == 0, "even", "odd" ), IIf( i > 2, Pair( i, IIf( i == 4, "last", "" ) ), i * i ) )
   NEXT
   ? Pair( IIf( Bump() > 1, Bump(), -1 ), Bump() )
   RETURN

FUNCTION Pair( p, q )
   RETURN "(" + Str2( p ) + "," + Str2( q ) + ")"

STATIC FUNCTION Str2( v )
   RETURN IIf( ValType( v ) == "N", LTrim( Str( v ) ), v )

STATIC FUNCTION Bump()
   STATIC n := 0
   n := n + 1
   RETURN n
