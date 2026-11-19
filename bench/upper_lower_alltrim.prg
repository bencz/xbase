// Case and blanks: Upper(), Lower(), AllTrim(), LTrim() and RTrim() of 1,000 mixed-case strings padded with blanks
// on both sides, 1,500,000 times.
STATIC s_nSeed := 8

PROCEDURE Main()
   LOCAL t, a := Array( 1000 ), i, j, x, w, n := 0, ok
   FOR i := 1 TO Len( a )
      w := ""
      FOR j := 1 TO 5 + Rand( 30 )
         w += SubStr( "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789", Rand( 37 ) + 1, 1 )
      NEXT
      a[ i ] := Space( Rand( 6 ) ) + "x" + w + "Q" + Space( Rand( 6 ) )
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 1500000
      x := a[ i % 1000 + 1 ]
      w := AllTrim( x )
      n += Len( w ) + Asc( Upper( w ) ) + Asc( Right( Lower( w ), 1 ) ) + Len( RTrim( x ) ) - Len( LTrim( x ) )
   NEXT
   ok := n == 320344500
   ? "upper_lower_alltrim", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
