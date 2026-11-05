// The runtime's ASort: 300,000 numbers in natural order, then 100,000 with a comparison block, 5 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), rep, ok := .T.
   FOR rep := 1 TO 5
      ok := ok .AND. SortBoth()
   NEXT
   ? "asort", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

FUNCTION SortBoth()
   LOCAL a := Array( 300000 ), b := Array( 100000 ), x := 777, i, sum := 0
   FOR i := 1 TO Len( a )
      x := ( x * 16807 ) % 2147483647
      a[ i ] := x % 1000000
   NEXT
   x := 4242
   FOR i := 1 TO Len( b )
      x := ( x * 16807 ) % 2147483647
      b[ i ] := x % 1000000
   NEXT
   ASort( a )
   ASort( b, , , {| p, q | p > q } )
   FOR i := 1 TO Len( a ) STEP 1000
      sum := sum + a[ i ]
   NEXT
   RETURN a[ 1 ] == 8 .AND. a[ 150000 ] == 500673 .AND. a[ 300000 ] == 999998 .AND. sum == 149589926 .AND. ;
      b[ 1 ] == 999994 .AND. b[ 50000 ] == 501282 .AND. b[ 100000 ] == 8
