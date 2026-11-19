// bench/monte_carlo_pi.prg in Harbour (its bit functions where xbase has bit operators).
// Pi by the Monte Carlo method: 50,000,000 points of the unit square from a linear congruential generator (31 bits),
// the count of those inside the quarter circle.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 50000000, seed := 42, i, x, y, inside := 0, est
   FOR i := 1 TO n
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      x := seed / 2147483648.0
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      y := seed / 2147483648.0
      IF x * x + y * y <= 1.0
         inside++
      ENDIF
   NEXT
   est := 4.0 * inside / n
   ? "monte_carlo_pi", ElapsedMs() - t, IIf( inside == 39267523 .AND. Abs( est - 3.14159 ) < 0.01, "ok", "WRONG" )
   RETURN
