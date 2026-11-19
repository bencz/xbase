// Nested arrays: a 100 x 100 x 100 cube made with Array( 100, 100, 100 ), filled through a[ i, j, k ], then 10 passes
// that change the cells from their neighbors along the last two dimensions and sum them.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := Array( 100, 100, 100 ), i, j, k, p, aPlane, aRow, nSum := 0, ok
   FOR i := 1 TO 100
      FOR j := 1 TO 100
         FOR k := 1 TO 100
            a[ i, j, k ] := ( i * 31 + j * 17 + k * 7 ) % 101
         NEXT
      NEXT
   NEXT
   FOR p := 1 TO 10
      FOR i := 1 TO 100
         aPlane := a[ i ]
         FOR j := 2 TO 100
            aRow := aPlane[ j ]
            FOR k := 2 TO 100
               aRow[ k ] := ( aRow[ k ] + aPlane[ j - 1, k ] + aRow[ k - 1 ] ) % 1000
            NEXT
         NEXT
      NEXT
      FOR i := 1 TO 100
         FOR j := 1 TO 100
            FOR k := 1 TO 100
               nSum += a[ i ][ j ][ k ]
            NEXT
         NEXT
      NEXT
   NEXT
   ok := nSum == 4905893027
   ? "array_nested", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
