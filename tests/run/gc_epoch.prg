// Line marks carry the epoch of the collection that set them, in a byte that wraps after 255 collections. Hundreds of
// young and full collections run here while strings of one and of several lines, and arrays that age and get
// promoted, stay alive among garbage; every kept value is checked byte for byte after each round, so a line taken as
// free while it still holds an object shows.
PROCEDURE Main()
   LOCAL keep := Array( 200 ), sizes := Array( 200 ), keys := Array( 200 ), olds := {}, seed := 777
   LOCAL round, i, j, s, bad := 0
   FOR i := 1 TO Len( keep )
      sizes[ i ] := i % 300 + 1
      keys[ i ] := i
      keep[ i ] := Value( i, sizes[ i ] )
   NEXT
   FOR round := 1 TO 700
      FOR j := 1 TO 20
         seed := ( seed * 1103515245 + 12345 ) % 2147483648
         i := Int( seed % Len( keep ) ) + 1
         sizes[ i ] := Int( seed % 600 ) + 1
         keys[ i ] := i + round
         keep[ i ] := Value( keys[ i ], sizes[ i ] )
         s := Replicate( "g", Int( seed % 400 ) + 1 )
      NEXT
      IF round % 7 == 0
         AAdd( olds, { round, Replicate( "o", round % 200 + 1 ) } )
      ENDIF
      __GcCollect( round % 50 != 0 )
      FOR i := 1 TO Len( keep )
         IF !( keep[ i ] == Value( keys[ i ], sizes[ i ] ) )
            bad++
         ENDIF
      NEXT
      FOR i := 1 TO Len( olds )
         IF !( olds[ i ][ 2 ] == Replicate( "o", olds[ i ][ 1 ] % 200 + 1 ) )
            bad++
         ENDIF
      NEXT
   NEXT
   ? "bad", bad, "rounds", round - 1, "olds", Len( olds )
RETURN

STATIC FUNCTION Value( i, n )
RETURN Left( Replicate( Chr( 65 + i % 26 ) + Chr( 97 + i % 23 ), Int( n / 2 ) + 1 ), n )
