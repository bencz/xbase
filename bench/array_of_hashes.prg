// Records as hashes: 120,000 hashes of four fields made from a fixed-seed generator, totals per group read by field
// name, a count of the records that pass a test, and the array sorted by a field with a comparison block.
STATIC s_nSeed := 31337

PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := Array( 120000 ), aTotal := Array( 50 ), i, r, nPass := 0, nSum := 0, ok
   AFill( aTotal, 0 )
   FOR i := 1 TO Len( a )
      a[ i ] := { "id" => i, "name" => "N" + StrZero( Rand( 100000 ), 5 ), "score" => Rand( 1000 ), ;
         "group" => Rand( 50 ) + 1 }
   NEXT
   FOR EACH r IN a
      aTotal[ r[ "group" ] ] += r[ "score" ]
      IF r[ "score" ] >= 500 .AND. Left( r[ "name" ], 2 ) == "N1"
         nPass++
      ENDIF
   NEXT
   ASort( a, , , {| x, y | x[ "score" ] < y[ "score" ] } )
   FOR i := 1 TO Len( a ) STEP 1000
      nSum += a[ i ][ "score" ]
   NEXT
   ok := aTotal[ 1 ] == 1239678 .AND. aTotal[ 50 ] == 1170281 .AND. nPass == 5950 .AND. ;
      nSum == 59429 .AND. a[ 1 ][ "score" ] == 0 .AND. a[ 120000 ][ "score" ] == 999
   ? "array_of_hashes", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
