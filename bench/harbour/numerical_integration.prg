// bench/numerical_integration.prg in Harbour (its bit functions where xbase has bit operators).
// Codeblocks called with doubles: Simpson's rule over 1,000,000 intervals for four integrals whose values are known
// (4 / (1 + x^2) and Sqrt(x) on [0, 1], Exp(-x^2) on [0, 1], 1 / x on [1, 2]), each function a codeblock, 3 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1000000, r1, r2, r3, r4, rep, ok := .T.
   FOR rep := 1 TO 3
      r1 := Simpson( {| x | 4.0 / ( 1.0 + x * x ) }, 0.0, 1.0, n )
      r2 := Simpson( {| x | Exp( -( x * x ) ) }, 0.0, 1.0, n )
      r3 := Simpson( {| x | 1.0 / x }, 1.0, 2.0, n )
      r4 := Simpson( {| x | Sqrt( x ) }, 0.0, 1.0, n )
      ok := ok .AND. Abs( r1 - 3.141592653589793 ) < 0.000000000001 .AND. ;
         Abs( r2 - 0.746824132812427 ) < 0.000000000001 .AND. Abs( r3 - 0.6931471805599453 ) < 0.000000000001 .AND. ;
         Abs( r4 - 2.0 / 3.0 ) < 0.000000001
   NEXT
   ? "numerical_integration", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The integral of bF from a to b by Simpson's rule over n intervals (n even).
STATIC FUNCTION Simpson( bF, a, b, n )
   LOCAL h := ( b - a ) / n, s := Eval( bF, a ) + Eval( bF, b ), i
   FOR i := 1 TO n - 1
      s := s + IIf( hb_bitAnd( i, 1 ) == 1, 4.0, 2.0 ) * Eval( bF, a + i * h )
   NEXT
   RETURN s * h / 3.0
