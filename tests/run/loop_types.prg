// Loops whose values change type, which the compiler checks where they enter the loop (guards) and whose first pass it
// runs ahead when it settles a value's type (peeling): the results, their types and how they show are those of the
// plain loop, whatever the values are. A sum that starts at the integer 0 and adds doubles, integers, or nothing (no
// pass at all: the integer 0 stays); integers that grow past 64 bits into doubles; FOR loops over a parameter that is
// an integer, a double, a date, a string or NIL (an error a handler substitutes), with a limit of every kind; a
// variable that alternates types; a nested loop whose inner sum restarts; a loop left by EXIT in its first pass; a sum
// of strings.
PROCEDURE Main()
   LOCAL x
   ? "sums:", Show( Basel( 1, 1000 ) ), Show( Basel( 5, 4 ) ), Show( IntSum( 1, 100 ) ), Show( Mixed( 10 ) )
   ? "big:", Show( Grow( 70 ) ), Show( Grow( 3 ) )
   ? "starts:", Show( Count( 1, 5 ) ), Show( Count( 1.5, 5 ) ), Show( Count( 2.5, 2.5 ) )
   ? "dates:", Show( Days( SToD( "20261116" ), SToD( "20261120" ) ) )
   ? "limits:", Show( Count( 1, 3.7 ) ), Show( Count( -2, 2 ) )
   ? "errors:", Recovered( "a", 3 ), Recovered( NIL, 3 ), Recovered( 1, "b" )
   ? "alternates:", Show( Alternate( 6 ) )
   ? "nested:", Show( Nested( 4 ) )
   ? "exit first:", Show( ExitFirst( 7 ) ), Show( ExitFirst( 0 ) )
   ? "strings:", Strings( 4 )
   x := Basel( 1, 0 )
   ? "empty:", ValType( x ), x, Str( x ), Len( Str( x ) )
   x := Basel( 1, 1 )
   ? "one:", ValType( x ), x, Str( x ), Len( Str( x ) )
RETURN

STATIC FUNCTION Show( x )
RETURN ValType( x ) + ":" + AllTrim( Str( x, 20, 8 ) ) + "/" + LTrim( Str( Len( Str( x ) ) ) )

STATIC FUNCTION Basel( nFrom, nTo )
   LOCAL n, nInv, nSum := 0
   FOR n := nFrom TO nTo
      nInv := 1 / n
      nSum += nInv * nInv
   NEXT
RETURN nSum

STATIC FUNCTION IntSum( nFrom, nTo )
   LOCAL n, nSum := 0
   FOR n := nFrom TO nTo
      nSum += n * n
   NEXT
RETURN nSum

// Integers, then doubles from the third pass on.
STATIC FUNCTION Mixed( nTo )
   LOCAL n, nSum := 0
   FOR n := 1 TO nTo
      nSum += iif( n > 2, n / 4, n )
   NEXT
RETURN nSum

// Doubling past 2^63: the integer becomes a double.
STATIC FUNCTION Grow( nTimes )
   LOCAL k, n := 1
   FOR k := 1 TO nTimes
      n := n * 2
   NEXT
RETURN n

STATIC FUNCTION Count( nFrom, nTo )
   LOCAL n, nSum := 0, nPasses := 0
   FOR n := nFrom TO nTo
      nSum += n
      nPasses++
   NEXT
RETURN nSum * 100 + nPasses

STATIC FUNCTION Days( dFrom, dTo )
   LOCAL d, n := 0
   FOR d := dFrom TO dTo
      n += Day( d )
   NEXT
RETURN n

// A start or a limit that is no number: the FOR's error, which the block substitutes.
STATIC FUNCTION Recovered( xFrom, xTo )
   LOCAL b := ErrorBlock( {| e | Break( e:Operation + " " + e:Description ) } ), c
   BEGIN SEQUENCE
      c := Show( Count( xFrom, xTo ) )
   RECOVER USING c
   END SEQUENCE
   ErrorBlock( b )
RETURN c

// A value that is an integer, then a double, then an integer again.
STATIC FUNCTION Alternate( nTo )
   LOCAL n, x := 0, nSum := 0
   FOR n := 1 TO nTo
      x := iif( n % 2 == 0, n / 2, n )
      nSum += x
   NEXT
RETURN nSum

STATIC FUNCTION Nested( nTo )
   LOCAL i, j, nIn, nAll := 0
   FOR i := 1 TO nTo
      nIn := 0
      FOR j := 1 TO i
         nIn += j / i
      NEXT
      nAll += nIn
   NEXT
RETURN nAll

STATIC FUNCTION ExitFirst( nTo )
   LOCAL n, nSum := 0
   FOR n := 1 TO nTo
      nSum += 0.5
      EXIT
   NEXT
RETURN nSum

STATIC FUNCTION Strings( nTo )
   LOCAL n, c := ""
   FOR n := 1 TO nTo
      c += Chr( 64 + n )
   NEXT
RETURN c
