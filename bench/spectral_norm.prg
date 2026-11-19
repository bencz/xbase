// The spectral norm of the infinite matrix A(i,j) = 1 / ((i+j)(i+j+1)/2 + i + 1) (the benchmarks game's), cut to
// 1,200 x 1,200: 10 rounds of the power method on A transposed times A, each element of A from a function call.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1200, u, v, w, i, vbv := 0.0, vv := 0.0, r
   u := AFill( Array( n ), 1.0 )
   v := AFill( Array( n ), 0.0 )
   w := AFill( Array( n ), 0.0 )
   FOR i := 1 TO 10
      MulAtAv( n, u, v, w )
      MulAtAv( n, v, u, w )
   NEXT
   FOR i := 1 TO n
      vbv += u[ i ] * v[ i ]
      vv += v[ i ] * v[ i ]
   NEXT
   r := Sqrt( vbv / vv )
   ? "spectral_norm", ElapsedMs() - t, IIf( Abs( r - 1.2742241501 ) < 0.000000001, "ok", "WRONG" )
   RETURN

// The element of A at the 0-based row i and column j.
STATIC FUNCTION EvalA( i, j )
   RETURN 1.0 / ( Int( ( i + j ) * ( i + j + 1 ) / 2 ) + i + 1 )

// v := A * u
STATIC PROCEDURE MulAv( n, u, v )
   LOCAL i, j, s
   FOR i := 1 TO n
      s := 0.0
      FOR j := 1 TO n
         s += EvalA( i - 1, j - 1 ) * u[ j ]
      NEXT
      v[ i ] := s
   NEXT
   RETURN

// v := A transposed * u
STATIC PROCEDURE MulAtv( n, u, v )
   LOCAL i, j, s
   FOR i := 1 TO n
      s := 0.0
      FOR j := 1 TO n
         s += EvalA( j - 1, i - 1 ) * u[ j ]
      NEXT
      v[ i ] := s
   NEXT
   RETURN

// v := A transposed * A * u, through w
STATIC PROCEDURE MulAtAv( n, u, v, w )
   MulAv( n, u, w )
   MulAtv( n, w, v )
   RETURN
