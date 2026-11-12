// Loops that take a slow path now and then: the iteration ends in the loop's generic copy, and the next one runs the
// fast paths again, in a second fast copy of the loop (codegen.loop_reenter). A total that turns decimal midway and
// back, a value that changes type every third iteration, a loop left by EXIT whose values are used after it, and a
// loop whose call runs a destructor (the pending request's slow path): every result is the same as without copies.
STATIC nGone := 0

PROCEDURE Main()
   ? Decimals( 10 ), Decimals( 0 )
   ? Swap( 30 ), Swap( 1 )
   ? Exits( 50 )
   ? Collected( 1000 ), nGone
RETURN

FUNCTION Decimals( n )
   LOCAL i, t := 0, k := 0
   FOR i := 1 TO n
      IF i == 5
         t += 0.5
      ELSEIF i == 8
         t := Int( t )
      ENDIF
      t += i
      k += i * 2
   NEXT
RETURN Str( t, 10, 2 ) + Str( k, 6 ) + " " + ValType( t )

FUNCTION Swap( n )
   LOCAL i, v, c := 0
   FOR i := 1 TO n
      v := IIf( i % 3 == 0, "xyz", i )
      IF ValType( v ) == "N"
         c += v
      ELSE
         c += Len( v )
      ENDIF
   NEXT
RETURN c

FUNCTION Exits( n )
   LOCAL i := 0, s := 0
   DO WHILE .T.
      i++
      IF i % 7 == 0
         s += 0.25
         LOOP
      ENDIF
      IF i > n
         EXIT
      ENDIF
      s += i
   ENDDO
RETURN Str( s, 10, 2 ) + Str( i, 6 )

FUNCTION Collected( n )
   LOCAL i, t := 0
   Drop()
   FOR i := 1 TO n
      IF i == 3
         __GcCollect()
      ENDIF
      t += i
   NEXT
RETURN t

STATIC PROCEDURE Drop()
   TDrop():New()
RETURN

CREATE CLASS TDrop
   EXPORTED:
   METHOD New
   DESTRUCTOR Gone
ENDCLASS

METHOD New() CLASS TDrop
   RETURN Self

METHOD PROCEDURE Gone() CLASS TDrop
   nGone++
   RETURN
