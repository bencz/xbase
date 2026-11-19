// Sieve of Atkin: the primes up to 2,000,000 from the three quadratic forms 4x^2+y^2, 3x^2+y^2 and 3x^2-y^2 taken
// modulo 12 (toggling an array of logicals), then the multiples of the squares of primes struck out; 12 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), limit := 2000000, a := Array( limit ), x, y, n, r, i, j, k, count, rep, ok := .T.
   FOR rep := 1 TO 12
      AFill( a, .F. )
      x := 1
      DO WHILE x * x <= limit
         y := 1
         DO WHILE y * y <= limit
            n := 4 * x * x + y * y
            IF n <= limit
               r := n % 12
               IF r == 1 .OR. r == 5
                  a[ n ] := ! a[ n ]
               ENDIF
            ENDIF
            n := 3 * x * x + y * y
            IF n <= limit .AND. n % 12 == 7
               a[ n ] := ! a[ n ]
            ENDIF
            IF x > y
               n := 3 * x * x - y * y
               IF n <= limit .AND. n % 12 == 11
                  a[ n ] := ! a[ n ]
               ENDIF
            ENDIF
            y++
         ENDDO
         x++
      ENDDO
      i := 5
      DO WHILE i * i <= limit
         IF a[ i ]
            k := i * i
            FOR j := k TO limit STEP k
               a[ j ] := .F.
            NEXT
         ENDIF
         i++
      ENDDO
      count := 2
      FOR i := 5 TO limit
         IF a[ i ]
            count++
         ENDIF
      NEXT
      ok := ok .AND. count == 148933
   NEXT
   ? "prime_sieve_atkin", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
