// Recursive quicksort written in xBase over 200,000 pseudo-random numbers (Park-Miller), 5 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), rep, ok := .T.
   FOR rep := 1 TO 5
      ok := ok .AND. SortRandom()
   NEXT
   ? "qsort", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

FUNCTION SortRandom()
   LOCAL n := 200000, a := Array( n ), x := 12345, i, sum := 0, sorted := .T.
   FOR i := 1 TO n
      x := ( x * 16807 ) % 2147483647
      a[ i ] := x % 1000000
   NEXT
   QSort( a, 1, n )
   FOR i := 2 TO n
      IF a[ i - 1 ] > a[ i ]
         sorted := .F.
      ENDIF
   NEXT
   FOR i := 1 TO n STEP 1000
      sum := sum + a[ i ]
   NEXT
   RETURN sorted .AND. a[ 1 ] == 6 .AND. a[ 100000 ] == 500294 .AND. a[ n ] == 999998 .AND. sum == 99556483

PROCEDURE QSort( a, lo, hi )
   LOCAL i, j, pivot, tmp
   DO WHILE lo < hi
      pivot := a[ Int( ( lo + hi ) / 2 ) ]
      i := lo
      j := hi
      DO WHILE i <= j
         DO WHILE a[ i ] < pivot
            i++
         ENDDO
         DO WHILE a[ j ] > pivot
            j--
         ENDDO
         IF i <= j
            tmp := a[ i ]
            a[ i ] := a[ j ]
            a[ j ] := tmp
            i++
            j--
         ENDIF
      ENDDO
      IF j - lo < hi - i
         QSort( a, lo, j )
         lo := i
      ELSE
         QSort( a, i, hi )
         hi := j
      ENDIF
   ENDDO
   RETURN
