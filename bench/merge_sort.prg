// Recursive top-down merge sort of 300,000 integers from a linear congruential generator, through one work array,
// 3 times with different seeds; the order and a weighted checksum of the sorted values are checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 300000, a := Array( n ), w := Array( n ), seed, i, rep, ok := .T., sum := 0
   FOR rep := 1 TO 3
      seed := rep
      FOR i := 1 TO n
         seed := Int( seed * 16807 % 2147483647 )
         a[ i ] := seed
      NEXT
      MergeSort( a, w, 1, n )
      FOR i := 2 TO n
         IF a[ i - 1 ] > a[ i ]
            ok := .F.
         ENDIF
      NEXT
      FOR i := 1 TO n
         sum += a[ i ] * ( Int( i % 8 ) + 1 )
      NEXT
   NEXT
   ? "merge_sort", ElapsedMs() - t, IIf( ok .AND. sum == 4349496350073402, "ok", "WRONG" )
   RETURN

// Sorts a[ lo .. hi ]; w is the work array the halves merge through.
STATIC PROCEDURE MergeSort( a, w, lo, hi )
   LOCAL mid, i, j, k
   IF hi - lo < 1
      RETURN
   ENDIF
   mid := Int( ( lo + hi ) / 2 )
   MergeSort( a, w, lo, mid )
   MergeSort( a, w, mid + 1, hi )
   i := lo
   j := mid + 1
   k := lo
   DO WHILE i <= mid .AND. j <= hi
      IF a[ j ] < a[ i ]
         w[ k ] := a[ j ]
         j++
      ELSE
         w[ k ] := a[ i ]
         i++
      ENDIF
      k++
   ENDDO
   DO WHILE i <= mid
      w[ k ] := a[ i ]
      i++
      k++
   ENDDO
   DO WHILE j <= hi
      w[ k ] := a[ j ]
      j++
      k++
   ENDDO
   FOR k := lo TO hi
      a[ k ] := w[ k ]
   NEXT
   RETURN
