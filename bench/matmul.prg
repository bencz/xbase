// Nested arrays: the product of two 100 x 100 integer matrices, 20 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 100, a, b, c, i, j, k, s, total := 0, rep
   a := Array( n, n )
   b := Array( n, n )
   c := Array( n, n )
   FOR i := 1 TO n
      FOR j := 1 TO n
         a[ i, j ] := ( i - 1 + j - 1 ) % 7
         b[ i, j ] := ( ( i - 1 ) * ( j - 1 ) ) % 5
      NEXT
   NEXT
   FOR rep := 1 TO 20
   FOR i := 1 TO n
      FOR j := 1 TO n
         s := 0
         FOR k := 1 TO n
            s := s + a[ i, k ] * b[ k, j ]
         NEXT
         c[ i, j ] := s
         total := total + s
      NEXT
   NEXT
   NEXT
   ? "matmul", ElapsedMs() - t, IIf( total == 20 * 4798200, "ok", "WRONG" )
   RETURN
