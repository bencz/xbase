// bench/black_scholes.prg in Harbour (its bit functions where xbase has bit operators).
// Black-Scholes prices of 500,000 European options (a call and a put each), their inputs from a linear congruential
// generator: the normal distribution by its polynomial approximation, Exp(), Log() and Sqrt(), the sum of the prices.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 500000, seed := 12345, i, s, x, tm, r := 0.02, v := 0.30
   LOCAL d1, d2, vst, disc, sumCall := 0.0, sumPut := 0.0, ok
   FOR i := 1 TO n
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      s := 5.0 + 25.0 * seed / 2147483648.0
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      x := 1.0 + 99.0 * seed / 2147483648.0
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      tm := 0.25 + 9.75 * seed / 2147483648.0
      vst := v * Sqrt( tm )
      d1 := ( Log( s / x ) + ( r + v * v * 0.5 ) * tm ) / vst
      d2 := d1 - vst
      disc := x * Exp( -r * tm )
      sumCall += s * Cnd( d1 ) - disc * Cnd( d2 )
      sumPut += disc * Cnd( -d2 ) - s * Cnd( -d1 )
   NEXT
   ok := Abs( sumCall / 1494483.92385431 - 1 ) < 0.000000001 .AND. Abs( sumPut / 15576648.71605097 - 1 ) < 0.000000001
   ? "black_scholes", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The cumulative normal distribution (the polynomial approximation of Abramowitz and Stegun, 26.2.17).
STATIC FUNCTION Cnd( d )
   LOCAL k := 1.0 / ( 1.0 + 0.2316419 * Abs( d ) ), w
   w := 1.0 - 0.3989422804014327 * Exp( -0.5 * d * d ) * ;
      k * ( 0.319381530 + k * ( -0.356563782 + k * ( 1.781477937 + k * ( -1.821255978 + k * 1.330274429 ) ) ) )
   RETURN IIf( d < 0.0, 1.0 - w, w )
