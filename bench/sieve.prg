// Array of logicals: the primes up to 2,000,000 (sieve of Eratosthenes), 10 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 2000000, a := Array( n ), i, j, count, rep, ok := .T.
   FOR rep := 1 TO 10
   count := 0
   FOR i := 2 TO n
      a[ i ] := .T.
   NEXT
   a[ 1 ] := .F.
   i := 2
   DO WHILE i * i <= n
      IF a[ i ]
         FOR j := i * i TO n STEP i
            a[ j ] := .F.
         NEXT
      ENDIF
      i++
   ENDDO
   FOR i := 1 TO n
      IF a[ i ]
         count++
      ENDIF
   NEXT
   ok := ok .AND. count == 148933
   NEXT
   ? "sieve", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
