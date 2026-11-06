// Typed continuations in functions whose loops join several checked operations (the copy's
// parameters where they meet): a quicksort, and nested loops with .AND. joins where the error
// block substitutes values of other types, entering the copy from several checks.
PROCEDURE Main()
   LOCAL a := { 5, 3, 9, 1, 7, 2, 8 }, x := 12345, i, b := Array( 50 )
   FOR i := 1 TO 50
      x := ( x * 16807 ) % 2147483647
      b[ i ] := x % 100
   NEXT
   QSort( a, 1, Len( a ) )
   QSort( b, 1, 50 )
   FOR i := 1 TO Len( a )
      QQOut( Str( a[ i ], 2 ) )
   NEXT
   ? b[ 1 ], b[ 25 ], b[ 50 ]
   ErrorBlock( {| e | IIf( e:genCode == 5, "s", Len( e:args ) ) } )
   FOR i := 0 TO 4
      x := Nested( i )
      ? i, x[ 1 ], x[ 2 ], x[ 3 ]
   NEXT
RETURN

STATIC FUNCTION Nested( n )
   LOCAL i, j, t := 0, u := 1, ok := .T.
   FOR i := 1 TO 3
      FOR j := 1 TO 3
         t := t + 6 / ( j - n )
         u := u * 2 - 12 / ( i - n + 1 )
         ok := ok .AND. t < 100 .AND. u > -50
      NEXT
   NEXT
RETURN { t, u, ok }
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
