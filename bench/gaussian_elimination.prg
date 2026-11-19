// Doubles in nested arrays: a 300 x 300 system of random doubles (a fixed seed) whose solution is all ones, solved by
// Gaussian elimination with partial pivoting (rows swapped as references) and back substitution, 6 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 300, a, aRow, aPiv, x, nSeed, s, f, nErr, i, j, k, p, rep, ok := .T.
   FOR rep := 1 TO 6
      nSeed := 42
      a := Array( n, n + 1 )
      FOR i := 1 TO n
         s := 0.0
         FOR j := 1 TO n
            nSeed := ( nSeed * 1103515245 + 12345 ) & 0x7FFFFFFF
            a[ i, j ] := nSeed / 2147483648 - 0.5
            s := s + a[ i, j ]
         NEXT
         a[ i, n + 1 ] := s
      NEXT
      FOR k := 1 TO n
         p := k
         FOR i := k + 1 TO n
            IF Abs( a[ i, k ] ) > Abs( a[ p, k ] )
               p := i
            ENDIF
         NEXT
         aPiv := a[ p ]
         a[ p ] := a[ k ]
         a[ k ] := aPiv
         FOR i := k + 1 TO n
            aRow := a[ i ]
            f := aRow[ k ] / aPiv[ k ]
            FOR j := k TO n + 1
               aRow[ j ] := aRow[ j ] - f * aPiv[ j ]
            NEXT
         NEXT
      NEXT
      x := Array( n )
      nErr := 0.0
      FOR i := n TO 1 STEP -1
         aRow := a[ i ]
         s := aRow[ n + 1 ]
         FOR j := i + 1 TO n
            s := s - aRow[ j ] * x[ j ]
         NEXT
         x[ i ] := s / aRow[ i ]
         nErr := Max( nErr, Abs( x[ i ] - 1.0 ) )
      NEXT
      ok := ok .AND. nErr < 0.000000001
   NEXT
   ? "gaussian_elimination", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
