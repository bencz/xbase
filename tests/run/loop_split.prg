// Loops whose fast copy is left in the middle of an iteration for the generic copy (a slow case: an integer
// overflowing into a double, an element of another type, a value changing type, an index out of the fast path's
// cases), with values used after the loop, EXIT and RETURN inside loops, nested loops, and a loop over a parameter
// passed by reference (its cell is followed on the fast path).
PROCEDURE Main()
   LOCAL i, j, n := 0, s := 0, a := { 1, 2, 3, "four", 5 }, big := 9223372036854775800, m
   FOR i := 1 TO 20
      big += 1
   NEXT
   ? big, i
   FOR i := 1 TO Len( a )
      IF ValType( a[ i ] ) == "N"
         s += a[ i ] * i
      ELSE
         s += Len( a[ i ] )
      ENDIF
   NEXT
   ? s
   FOR i := 1 TO 10
      n := n + IIf( i == 5, 0.5, 1 )
   NEXT
   ? n
   i := 0
   n := 0
   DO WHILE .T.
      i++
      n += i * 1.5
      IF n > 100
         EXIT
      ENDIF
   ENDDO
   ? i, n
   m := { { 1, 2, 3 }, { 4, 5.5, 6 }, { 7, 8, "9" } }
   s := 0
   FOR i := 1 TO 3
      FOR j := 1 TO 3
         s += IIf( ValType( m[ i, j ] ) == "N", m[ i, j ], Val( m[ i, j ] ) )
      NEXT
   NEXT
   ? s, i, j
   ? Find( { 3, 4.5, "z", 7 }, 7 ), Find( { 1, 2 }, 9 ), Sum( { 1, 2, 3 }, 1.5 ), Sum( { 1, 2, 3 }, 2 )
   n := 0
   Inc( @n, 10 )
   s := 0.5
   Inc( @s, 3 )
   ? n, s
   RETURN

PROCEDURE Inc( v, k )
   LOCAL i
   FOR i := 1 TO k
      v += i
   NEXT
   RETURN

FUNCTION Find( a, x )
   LOCAL i
   FOR i := 1 TO Len( a )
      IF ValType( a[ i ] ) == ValType( x ) .AND. a[ i ] == x
         RETURN i
      ENDIF
   NEXT
   RETURN 0

FUNCTION Sum( a, k )
   LOCAL i, t := 0
   FOR i := 1 TO Len( a )
      t += a[ i ] * k
   NEXT
   RETURN t
