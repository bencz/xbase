// DGEMM: C := alpha A B + beta C on 250 x 250 matrices of doubles in flat row-major arrays (loops in i, k, j order),
// the inputs from a linear congruential generator, 3 times; the check is the sum and the trace of C.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 250, alpha := 1.5, beta := 0.5, a, b, c, seed := 7, i, j, k, ci, bk, aik, rep
   LOCAL sum := 0.0, trace := 0.0, ok
   a := Array( n * n )
   b := Array( n * n )
   c := Array( n * n )
   FOR i := 1 TO n * n
      seed := ( seed * 1103515245 + 12345 ) & 0x7FFFFFFF
      a[ i ] := seed / 1073741824.0 - 1.0
      seed := ( seed * 1103515245 + 12345 ) & 0x7FFFFFFF
      b[ i ] := seed / 1073741824.0 - 1.0
      seed := ( seed * 1103515245 + 12345 ) & 0x7FFFFFFF
      c[ i ] := seed / 1073741824.0 - 1.0
   NEXT
   FOR rep := 1 TO 3
      FOR i := 0 TO n - 1
         ci := i * n
         FOR j := ci + 1 TO ci + n
            c[ j ] := beta * c[ j ]
         NEXT
         FOR k := 0 TO n - 1
            aik := alpha * a[ ci + k + 1 ]
            bk := k * n - ci
            FOR j := ci + 1 TO ci + n
               c[ j ] += aik * b[ bk + j ]
            NEXT
         NEXT
      NEXT
   NEXT
   FOR i := 1 TO n
      FOR j := 1 TO n
         sum += c[ ( i - 1 ) * n + j ]
      NEXT
      trace += c[ ( i - 1 ) * n + i ]
   NEXT
   ok := Abs( sum + 145.62810535074 ) < 0.000001 .AND. Abs( trace - 342.80906789597 ) < 0.000001
   ? "dgemm", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
