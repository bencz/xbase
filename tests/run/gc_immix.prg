// The heap under churn: strings of one line, of several lines (above a line they may go to an overflow block) and
// large ones (above 8 KB, allocated on their own), kept in an array whose elements are replaced at random among
// garbage, through young and full collections; arrays of arrays age and get promoted. Every kept value is checked
// byte for byte at the end, so a line freed while an object still covers it shows.
PROCEDURE Main()
   LOCAL keep := Array( 3000 ), sizes := Array( 3000 ), nested := {}, seed := 12345, i, k, n, s, bad := 0, total := 0
   FOR i := 1 TO Len( keep )
      sizes[ i ] := Size( i, 0 )
      keep[ i ] := Value( i, sizes[ i ] )
   NEXT
   FOR k := 1 TO 40000
      seed := ( seed * 1103515245 + 12345 ) % 2147483648
      i := Int( seed % Len( keep ) ) + 1
      sizes[ i ] := Size( i, k )
      keep[ i ] := Value( i, sizes[ i ] )
      s := Replicate( "x", Int( seed % 700 ) + 1 ) // garbage of every size
      IF k % 97 == 0
         AAdd( nested, { k, Replicate( "n", k % 50 + 1 ), { k * 2 } } )
      ENDIF
      IF k % 5000 == 0
         __GcCollect( .T. )
      ENDIF
      IF k % 13000 == 0
         __GcCollect()
      ENDIF
   NEXT
   FOR i := 1 TO Len( keep )
      IF !( keep[ i ] == Value( i, sizes[ i ] ) )
         bad++
      ENDIF
      total += Len( keep[ i ] )
   NEXT
   n := 0
   FOR i := 1 TO Len( nested )
      k := nested[ i ][ 1 ]
      IF nested[ i ][ 2 ] == Replicate( "n", k % 50 + 1 ) .AND. nested[ i ][ 3 ][ 1 ] == k * 2
         n++
      ENDIF
   NEXT
   ? "bad", bad, "total", total, "nested", n, Len( nested )
RETURN

// Mostly short strings, some of several lines, a few large.
STATIC FUNCTION Size( i, k )
   LOCAL r := ( i * 7 + k * 13 ) % 1000
RETURN IIF( r < 900, r % 120 + 1, IIF( r < 995, 130 + r * 7, 9000 + r ) )

STATIC FUNCTION Value( i, n )
RETURN Left( Replicate( Chr( 65 + i % 26 ) + Chr( 97 + i % 23 ), Int( n / 2 ) + 1 ), n )
