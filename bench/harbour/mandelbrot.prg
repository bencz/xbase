// bench/mandelbrot.prg in Harbour (its bit functions where xbase has bit operators).
// The Mandelbrot set as a bitmap (the benchmarks game's form): 1,600 x 1,600 points, at most 50 iterations each, one
// bit per point packed into bytes (a row padded to a whole byte), the bytes folded into a checksum.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1600, x, y, cr, ci, zr, zi, tr, ti, k, bits, nBits, cs := 0, nBytes := 0
   FOR y := 0 TO n - 1
      ci := 2.0 * y / n - 1.0
      bits := 0
      nBits := 0
      FOR x := 0 TO n - 1
         cr := 2.0 * x / n - 1.5
         zr := 0.0
         zi := 0.0
         tr := 0.0
         ti := 0.0
         k := 0
         DO WHILE k < 50 .AND. tr + ti <= 4.0
            zi := 2.0 * zr * zi + ci
            zr := tr - ti + cr
            tr := zr * zr
            ti := zi * zi
            k++
         ENDDO
         bits := hb_bitOr( hb_bitShift( bits, 1 ), IIf( tr + ti <= 4.0, 1, 0 ) )
         nBits++
         IF nBits == 8 .OR. x == n - 1
            bits := hb_bitShift( bits, 8 - nBits )
            cs := hb_bitAnd( cs * 31 + bits, 0x7FFFFFFF )
            nBytes++
            bits := 0
            nBits := 0
         ENDIF
      NEXT
   NEXT
   ? "mandelbrot", ElapsedMs() - t, IIf( cs == 366763831 .AND. nBytes == n * Int( ( n + 7 ) / 8 ), "ok", "WRONG" )
   RETURN
