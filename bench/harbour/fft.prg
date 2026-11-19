// bench/fft.prg in Harbour (its bit functions where xbase has bit operators).
// The fast Fourier transform (iterative radix 2, in place, on arrays of real and imaginary parts): 65,536 complex
// points from a linear congruential generator, transformed forward and back 10 times. The twiddle factors come from
// Cos() and Sin() of 2 pi / 2^k and complex multiplication; the check is the spectrum's sum of magnitudes and the
// round trip's largest error.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), logN := 16, n, re, im, re0, im0, cosT, sinT, i, rep, seed := 2024
   LOCAL spec := 0.0, maxErr := 0.0, ok
   n := hb_bitShift( 1, logN )
   re := Array( n )
   im := Array( n )
   FOR i := 1 TO n
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      re[ i ] := seed / 1073741824.0 - 1.0
      seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
      im[ i ] := seed / 1073741824.0 - 1.0
   NEXT
   re0 := AClone( re )
   im0 := AClone( im )
   // cos and sin of 2 pi / 2^k, for k from 1
   cosT := Array( logN )
   sinT := Array( logN )
   FOR i := 1 TO logN
      cosT[ i ] := Cos( 2 * Pi() / hb_bitShift( 1, i ) )
      sinT[ i ] := Sin( 2 * Pi() / hb_bitShift( 1, i ) )
   NEXT
   FOR rep := 1 TO 10
      Fft( re, im, n, cosT, sinT, -1.0 )
      IF rep == 1
         FOR i := 1 TO n
            spec += Abs( re[ i ] ) + Abs( im[ i ] )
         NEXT
      ENDIF
      Fft( re, im, n, cosT, sinT, 1.0 )
      FOR i := 1 TO n
         re[ i ] := re[ i ] / n
         im[ i ] := im[ i ] / n
      NEXT
   NEXT
   FOR i := 1 TO n
      maxErr := Max( maxErr, Max( Abs( re[ i ] - re0[ i ] ), Abs( im[ i ] - im0[ i ] ) ) )
   NEXT
   ok := Abs( spec / 15471706.35354992 - 1 ) < 0.000000001 .AND. maxErr < 0.000000001
   ? "fft", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The transform of re + i im in place; nSign -1 forward, 1 backward (unscaled).
STATIC PROCEDURE Fft( re, im, n, cosT, sinT, nSign )
   LOCAL i, j := 0, k, len, half, st, wr, wi, wr0, wi0, xr, xi, tmp
   FOR i := 0 TO n - 2
      IF i < j
         tmp := re[ i + 1 ]
         re[ i + 1 ] := re[ j + 1 ]
         re[ j + 1 ] := tmp
         tmp := im[ i + 1 ]
         im[ i + 1 ] := im[ j + 1 ]
         im[ j + 1 ] := tmp
      ENDIF
      k := hb_bitShift( n, -1 )
      DO WHILE k <= j
         j -= k
         k := hb_bitShift( k, -1 )
      ENDDO
      j += k
   NEXT
   len := 2
   st := 1
   DO WHILE len <= n
      half := hb_bitShift( len, -1 )
      wr0 := cosT[ st ]
      wi0 := nSign * sinT[ st ]
      FOR i := 1 TO n STEP len
         wr := 1.0
         wi := 0.0
         FOR j := i TO i + half - 1
            k := j + half
            xr := re[ k ] * wr - im[ k ] * wi
            xi := re[ k ] * wi + im[ k ] * wr
            re[ k ] := re[ j ] - xr
            im[ k ] := im[ j ] - xi
            re[ j ] += xr
            im[ j ] += xi
            tmp := wr * wr0 - wi * wi0
            wi := wr * wi0 + wi * wr0
            wr := tmp
         NEXT
      NEXT
      len := hb_bitShift( len, 1 )
      st++
   ENDDO
   RETURN
