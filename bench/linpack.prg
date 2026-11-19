// LINPACK: a dense 300 x 300 system (the original's matrix generator) solved by LU factorization with partial
// pivoting (DGEFA, DGESL, through DAXPY, IDAMAX and DSCAL on one column-major array), 5 times; the check is the
// solution, all ones, and the residual of A x - b.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 300, a, b, x, ipvt, rep, i, j, maxErr, resid, s, ok := .T.
   FOR rep := 1 TO 5
      a := Array( n * n )
      b := Array( n )
      ipvt := Array( n )
      MatGen( a, b, n )
      x := AClone( b )
      DGeFa( a, n, ipvt )
      DGeSl( a, n, ipvt, x )
      // the factorization overwrote a: a new copy gives the residual
      MatGen( a, b, n )
      maxErr := 0.0
      resid := 0.0
      FOR i := 1 TO n
         maxErr := Max( maxErr, Abs( x[ i ] - 1.0 ) )
         s := -b[ i ]
         FOR j := 1 TO n
            s += a[ ( j - 1 ) * n + i ] * x[ j ]
         NEXT
         resid := Max( resid, Abs( s ) )
      NEXT
      ok := ok .AND. maxErr < 0.000000001 .AND. resid < 0.000000001
   NEXT
   ? "linpack", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The original's test matrix: entries from a congruential sequence in [-2, 2), b the sums of the rows.
STATIC PROCEDURE MatGen( a, b, n )
   LOCAL init := 1325, i, j
   FOR j := 1 TO n
      FOR i := 1 TO n
         init := ( 3125 * init ) % 65536
         a[ ( j - 1 ) * n + i ] := ( init - 32768.0 ) / 16384.0
      NEXT
   NEXT
   AFill( b, 0.0 )
   FOR j := 1 TO n
      FOR i := 1 TO n
         b[ i ] += a[ ( j - 1 ) * n + i ]
      NEXT
   NEXT
   RETURN

// LU factorization of the column-major n x n matrix a in place, the pivot rows in ipvt.
STATIC PROCEDURE DGeFa( a, n, ipvt )
   LOCAL k, l, j, t, col
   FOR k := 1 TO n - 1
      col := ( k - 1 ) * n
      l := IDAMax( n - k + 1, a, col + k ) + k - 1
      ipvt[ k ] := l
      IF a[ col + l ] != 0.0
         IF l != k
            t := a[ col + l ]
            a[ col + l ] := a[ col + k ]
            a[ col + k ] := t
         ENDIF
         t := -1.0 / a[ col + k ]
         DScal( n - k, t, a, col + k + 1 )
         FOR j := k + 1 TO n
            t := a[ ( j - 1 ) * n + l ]
            IF l != k
               a[ ( j - 1 ) * n + l ] := a[ ( j - 1 ) * n + k ]
               a[ ( j - 1 ) * n + k ] := t
            ENDIF
            DAxpy( n - k, t, a, col + k + 1, a, ( j - 1 ) * n + k + 1 )
         NEXT
      ENDIF
   NEXT
   ipvt[ n ] := n
   RETURN

// Solves a x = b with the factors of DGeFa; b becomes x.
STATIC PROCEDURE DGeSl( a, n, ipvt, b )
   LOCAL k, l, t
   FOR k := 1 TO n - 1
      l := ipvt[ k ]
      t := b[ l ]
      IF l != k
         b[ l ] := b[ k ]
         b[ k ] := t
      ENDIF
      DAxpy( n - k, t, a, ( k - 1 ) * n + k + 1, b, k + 1 )
   NEXT
   FOR k := n TO 1 STEP -1
      b[ k ] := b[ k ] / a[ ( k - 1 ) * n + k ]
      t := -b[ k ]
      DAxpy( k - 1, t, a, ( k - 1 ) * n + 1, b, 1 )
   NEXT
   RETURN

// dy[ y .. y + n - 1 ] += da * dx[ x .. x + n - 1 ]
STATIC PROCEDURE DAxpy( n, da, dx, x, dy, y )
   LOCAL i
   IF n > 0 .AND. da != 0.0
      FOR i := 0 TO n - 1
         dy[ y + i ] += da * dx[ x + i ]
      NEXT
   ENDIF
   RETURN

// dx[ x .. x + n - 1 ] *= da
STATIC PROCEDURE DScal( n, da, dx, x )
   LOCAL i
   FOR i := x TO x + n - 1
      dx[ i ] := da * dx[ i ]
   NEXT
   RETURN

// The 1-based position of the largest magnitude in dx[ x .. x + n - 1 ] (the first of equal ones).
STATIC FUNCTION IDAMax( n, dx, x )
   LOCAL i, imax := 1, dmax := Abs( dx[ x ] ), v
   FOR i := 2 TO n
      v := Abs( dx[ x + i - 1 ] )
      IF v > dmax
         imax := i
         dmax := v
      ENDIF
   NEXT
   RETURN imax
