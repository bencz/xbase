// Heap sort in place (a binary max-heap built bottom-up, then sifted down) of 300,000 integers from a linear
// congruential generator, 3 times with different seeds; the order and a weighted checksum are checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 300000, a := Array( n ), seed, i, rep, ok := .T., sum := 0, x
   FOR rep := 1 TO 3
      seed := rep + 10
      FOR i := 1 TO n
         seed := Int( seed * 16807 % 2147483647 )
         a[ i ] := seed
      NEXT
      FOR i := Int( n / 2 ) TO 1 STEP -1
         SiftDown( a, i, n )
      NEXT
      FOR i := n TO 2 STEP -1
         x := a[ 1 ]
         a[ 1 ] := a[ i ]
         a[ i ] := x
         SiftDown( a, 1, i - 1 )
      NEXT
      FOR i := 2 TO n
         IF a[ i - 1 ] > a[ i ]
            ok := .F.
         ENDIF
      NEXT
      FOR i := 1 TO n
         sum += a[ i ] * ( Int( i % 8 ) + 1 )
      NEXT
   NEXT
   ? "heap_sort", ElapsedMs() - t, IIf( ok .AND. sum == 4351668891394344, "ok", "WRONG" )
   RETURN

// Moves a[ i ] down the heap a[ 1 .. n ] until neither child is larger.
STATIC PROCEDURE SiftDown( a, i, n )
   LOCAL x := a[ i ], c
   DO WHILE ( c := i * 2 ) <= n
      IF c < n .AND. a[ c + 1 ] > a[ c ]
         c++
      ENDIF
      IF a[ c ] <= x
         EXIT
      ENDIF
      a[ i ] := a[ c ]
      i := c
   ENDDO
   a[ i ] := x
   RETURN
