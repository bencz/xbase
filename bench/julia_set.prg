// The Julia set of c = -0.8 + 0.156i: escape times over a 1,600 x 1,200 grid of [-1.5, 1.5] x [-1, 1], at most 255
// iterations per point, counted in a histogram of 256 bins folded into a checksum.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), w := 1600, h := 1200, maxIter := 255, cr := -0.8, ci := 0.156
   LOCAL x, y, zr, zi, zr2, zi2, k, hist := AFill( Array( maxIter + 1 ), 0 ), total := 0, cs := 0
   FOR y := 0 TO h - 1
      FOR x := 0 TO w - 1
         zr := x * 3.0 / w - 1.5
         zi := y * 2.0 / h - 1.0
         k := 0
         DO WHILE k < maxIter
            zr2 := zr * zr
            zi2 := zi * zi
            IF zr2 + zi2 > 4.0
               EXIT
            ENDIF
            zi := 2.0 * zr * zi + ci
            zr := zr2 - zi2 + cr
            k++
         ENDDO
         hist[ k + 1 ]++
         total += k
      NEXT
   NEXT
   FOR k := 1 TO maxIter + 1
      cs := ( cs * 31 + hist[ k ] ) % 2147483648
   NEXT
   ? "julia_set", ElapsedMs() - t, IIf( total == 69243973 .AND. cs == 2105097618, "ok", "WRONG" )
   RETURN
