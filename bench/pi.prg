// Digits of pi from a program written for a tape machine (a C translation of a Brainfuck program): an array of
// 128000 integer cells, a cell index moved by small steps, and deeply nested loops that run while a cell is not zero.
// Without arguments it computes 30 digits, checks them and prints the benchmark line; `bench_pi <n>` prints n digits
// the way the original does.
PROCEDURE Main( cDigits )
   LOCAL t := ElapsedMs(), nDigits, cOut
   IF ValType( cDigits ) == "C"
      nDigits := Val( cDigits )
      ?? "Numero de casas: " + LTrim( Str( nDigits ) ) + Chr( 10 ) + PiDigits( nDigits ) + Chr( 10 )
      RETURN
   ENDIF
   cOut := PiDigits( 30 )
   ? "pi", ElapsedMs() - t, IIf( cOut == "3.1415926535897932384626433832" + Chr( 10 ), "ok", "WRONG" )
   RETURN

// The digits as the original prints them: "3." and the decimals, then a line break.
STATIC FUNCTION PiDigits( nDigits )
   LOCAL x := Array( 128000 ), xc, cOut := ""
   FOR xc := 1 TO Len( x )
      x[ xc ] := 0
   NEXT
   xc := 1
   xc++
   x[ xc ] += nDigits
   DO WHILE x[ xc ] != 0
      xc--
      x[ xc ]++
      xc += 8
      x[ xc ] += 10
      xc -= 7
      x[ xc ]--
   ENDDO
   xc++
   x[ xc ] += 5
   DO WHILE x[ xc ] != 0
      xc--
      x[ xc ] += 9
      xc++
      x[ xc ]--
   ENDDO
   x[ xc ]++
   xc += 6
   x[ xc ]++
   DO WHILE x[ xc ] != 0
      xc -= 2
      x[ xc ] += 3
      DO WHILE x[ xc ] != 0
         xc += 2
         DO WHILE x[ xc ] != 0
            x[ xc ]--
            xc--
         ENDDO
         xc--
         DO WHILE x[ xc ] != 0
            xc++
         ENDDO
         xc--
         x[ xc ]--
      ENDDO
      xc += 2
      DO WHILE x[ xc ] != 0
         xc++
         x[ xc ]++
         xc++
      ENDDO
      xc--
      DO WHILE x[ xc ] != 0
         xc--
      ENDDO
      xc++
   ENDDO
   xc++
   DO WHILE x[ xc ] != 0
      DO WHILE x[ xc ] != 0
         x[ xc ]--
         xc += 4
         x[ xc ]++
         xc -= 4
      ENDDO
      xc += 3
      x[ xc ] += 3
      xc++
      x[ xc ]--
   ENDDO
   xc--
   DO WHILE x[ xc ] != 0
      xc -= 4
   ENDDO
   xc -= 8
   x[ xc ]++
   DO WHILE x[ xc ] != 0
      x[ xc ]--
      xc += 12
      DO WHILE x[ xc ] != 0
         xc--
         x[ xc ]++
         DO WHILE x[ xc ] != 0
            x[ xc ]--
            xc += 4
            x[ xc ]++
            xc -= 4
         ENDDO
         xc++
         xc += 4
      ENDDO
      xc -= 4
      DO WHILE x[ xc ] != 0
         xc += 5
         DO WHILE x[ xc ] != 0
            xc -= 4
            x[ xc ]++
            xc += 4
            x[ xc ]--
         ENDDO
         xc -= 5
         x[ xc ]--
         DO WHILE x[ xc ] != 0
            xc -= 2
            x[ xc ] += 10
            xc += 2
            x[ xc ]--
         ENDDO
         xc += 3
         DO WHILE x[ xc ] != 0
            xc -= 2
            DO WHILE x[ xc ] != 0
               xc--
               x[ xc ]++
               xc -= 2
               x[ xc ]++
               xc += 3
               x[ xc ]--
            ENDDO
            xc--
            DO WHILE x[ xc ] != 0
               xc++
               x[ xc ]++
               xc--
               x[ xc ]--
            ENDDO
            xc--
            x[ xc ] += 2
            xc -= 2
            x[ xc ]++
            xc += 6
            x[ xc ]--
         ENDDO
         xc -= 2
         DO WHILE x[ xc ] != 0
            x[ xc ]--
         ENDDO
         xc -= 2
         x[ xc ]--
         xc--
         DO WHILE x[ xc ] != 0
            x[ xc ]--
            xc += 2
            x[ xc ]++
            xc--
            x[ xc ]--
            DO WHILE x[ xc ] != 0
               xc += 3
            ENDDO
            xc++
            DO WHILE x[ xc ] != 0
               DO WHILE x[ xc ] != 0
                  xc--
                  x[ xc ]++
                  xc++
                  x[ xc ]--
               ENDDO
               xc++
               x[ xc ]++
               xc += 2
            ENDDO
            xc -= 5
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            x[ xc ]--
         ENDDO
         xc++
         x[ xc ]++
         xc -= 3
         x[ xc ]--
         DO WHILE x[ xc ] != 0
            xc += 2
            x[ xc ]++
            xc -= 2
            x[ xc ]--
         ENDDO
         xc--
      ENDDO
      xc -= 4
      x[ xc ]++
      xc += 7
      xc++
      DO WHILE x[ xc ] != 0
         x[ xc ]--
      ENDDO
      xc++
      DO WHILE x[ xc ] != 0
         xc -= 3
         x[ xc ]++
         xc += 3
         x[ xc ]--
      ENDDO
      xc -= 2
      x[ xc ] += 10
      xc--
      DO WHILE x[ xc ] != 0
         x[ xc ]--
         xc += 2
         x[ xc ]++
         xc--
         x[ xc ]--
         DO WHILE x[ xc ] != 0
            xc += 3
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            DO WHILE x[ xc ] != 0
               xc--
               x[ xc ]++
               xc++
               x[ xc ]--
            ENDDO
            xc++
            x[ xc ]++
            xc += 2
         ENDDO
         xc -= 5
      ENDDO
      xc++
      DO WHILE x[ xc ] != 0
         x[ xc ]--
      ENDDO
      xc++
      x[ xc ]++
      xc++
      DO WHILE x[ xc ] != 0
         xc -= 2
         x[ xc ]++
         xc--
         x[ xc ]++
         xc += 3
         x[ xc ]--
      ENDDO
      xc -= 3
      xc--
      x[ xc ]++
      xc--
      x[ xc ]++
      xc += 2
      DO WHILE x[ xc ] != 0
         x[ xc ]--
         DO WHILE x[ xc ] != 0
            x[ xc ]--
            DO WHILE x[ xc ] != 0
               x[ xc ]--
               DO WHILE x[ xc ] != 0
                  x[ xc ]--
                  DO WHILE x[ xc ] != 0
                     x[ xc ]--
                     DO WHILE x[ xc ] != 0
                        x[ xc ]--
                        DO WHILE x[ xc ] != 0
                           x[ xc ]--
                           DO WHILE x[ xc ] != 0
                              x[ xc ]--
                              DO WHILE x[ xc ] != 0
                                 x[ xc ]--
                                 xc--
                                 x[ xc ]--
                                 xc++
                                 DO WHILE x[ xc ] != 0
                                    x[ xc ]--
                                    xc--
                                    x[ xc ]++
                                    xc--
                                    x[ xc ]--
                                    xc += 2
                                 ENDDO
                              ENDDO
                           ENDDO
                        ENDDO
                     ENDDO
                  ENDDO
               ENDDO
            ENDDO
         ENDDO
      ENDDO
      xc--
      DO WHILE x[ xc ] != 0
         x[ xc ] += 5
         DO WHILE x[ xc ] != 0
            xc -= 3
            x[ xc ] += 8
            xc--
            x[ xc ] += 8
            xc += 4
            x[ xc ]--
         ENDDO
         xc--
         xc -= 3
         x[ xc ]++
         xc--
         x[ xc ]--
         xc += 4
         DO WHILE x[ xc ] != 0
            xc++
            x[ xc ]++
            xc -= 3
            x[ xc ] += 9
            xc--
            x[ xc ]--
            xc += 3
            x[ xc ]--
         ENDDO
         xc -= 5
         DO WHILE x[ xc ] != 0
            xc += 2
            x[ xc ]++
            xc -= 2
            x[ xc ]--
         ENDDO
         x[ xc ]++
         xc--
         DO WHILE x[ xc ] != 0
            x[ xc ]--
            xc++
            x[ xc ]--
            xc--
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            xc += 2
            cOut += Chr( x[ xc ] )
            xc -= 4
            DO WHILE x[ xc ] != 0
               x[ xc ]++
               cOut += Chr( x[ xc ] )
               DO WHILE x[ xc ] != 0
                  x[ xc ]--
               ENDDO
            ENDDO
            xc += 2
            x[ xc ]--
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            xc += 2
            cOut += Chr( x[ xc ] )
            xc -= 2
            x[ xc ]--
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            x[ xc ]--
         ENDDO
         xc++
         DO WHILE x[ xc ] != 0
            x[ xc ]--
         ENDDO
         xc += 3
         DO WHILE x[ xc ] != 0
            xc += 2
            DO WHILE x[ xc ] != 0
               xc -= 8
               x[ xc ]++
               xc += 8
               x[ xc ]--
            ENDDO
            xc -= 2
            x[ xc ]--
         ENDDO
      ENDDO
      xc += 2
      DO WHILE x[ xc ] != 0
         x[ xc ]--
      ENDDO
      xc -= 3
      DO WHILE x[ xc ] != 0
         x[ xc ]--
      ENDDO
      xc -= 8
   ENDDO

   x[ xc ] += 10
   cOut += Chr( x[ xc ] )
   RETURN cOut
