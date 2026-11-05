// Doubles: iterations of the Mandelbrot set over a 250 x 250 grid, at most 100 per point, 6 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), size := 250, maxIter := 100, x, y, cr, ci, zr, zi, zr2, zi2, k, total := 0, rep
   FOR rep := 1 TO 6
      FOR y := 0 TO size - 1
         ci := y * 2.0 / size - 1.0
         FOR x := 0 TO size - 1
            cr := x * 3.0 / size - 2.0
            zr := 0.0
            zi := 0.0
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
            total += k
         NEXT
      NEXT
   NEXT
   ? "float", ElapsedMs() - t, IIf( total == 11391384, "ok", "WRONG" )
   RETURN
