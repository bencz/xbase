// Objects that only dead values held are collected (their destructors run): a chain built in a loop by an inlined
// function, then dropped; objects made in a loop, the last one dropped after it; values of a loop nested in another;
// values of loops that flow into another loop, a FOR and a DO WHILE left by EXIT; values of a loop left by BREAK into
// the RECOVER of a sequence around it (its unwind edge). A value's frame slot is cleared where its loop ends, so that
// what it held is not kept until the slot is reused.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL k := NIL, i, j, o, n := 0, c := ""
   FOR i := 1 TO 5
      k := Chain( k, i )
   NEXT
   k := NIL
   __GcCollect()
   ? "chain", s_nGone
   FOR i := 1 TO 4
      o := Node():New( i )
   NEXT
   o := NIL
   __GcCollect()
   ? "loop", s_nGone
   FOR i := 1 TO 2
      FOR j := 1 TO 3
         k := Chain( NIL, j )
      NEXT
      k := NIL
   NEXT
   __GcCollect()
   ? "nested", s_nGone
   s_nGone := 0
   FOR i := 1 TO 3
      n += Chain( NIL, i ):n
   NEXT
   FOR j := 1 TO 3
      __GcCollect()
      c += Str( s_nGone, 2 )
   NEXT
   i := 0
   DO WHILE .T.
      n += Chain( Chain( NIL, i ), i ):n
      IF ++i == 4
         EXIT
      ENDIF
   ENDDO
   DO WHILE j > 0
      __GcCollect()
      c += Str( s_nGone, 3 )
      j--
   ENDDO
   ? "next loop", n, c
   s_nGone := 0
   n := 0
   BEGIN SEQUENCE
      FOR i := 1 TO 3
         n += Sum3( Chain( NIL, i ), Chain( NIL, i + 10 ), Chain( NIL, i + 20 ) )
         IF i == 3
            Leave()
         ENDIF
      NEXT
   RECOVER
      __GcCollect()
      c := Str( s_nGone, 3 )
   END SEQUENCE
   __GcCollect()
   ? "left by BREAK", n, c, s_nGone
RETURN

STATIC FUNCTION Sum3( a, b, c )
RETURN a:n + b:n + c:n

STATIC PROCEDURE Leave()
   Break( NIL )
RETURN

FUNCTION Chain( prev, i )
   LOCAL o := Node():New( i )
   o:prev := prev
RETURN o

CREATE CLASS Node
   EXPORTED:
   VAR n
   VAR prev
   METHOD New
   DESTRUCTOR Bye
ENDCLASS

METHOD New( n ) CLASS Node
   ::n := n
RETURN Self

METHOD PROCEDURE Bye() CLASS Node
   s_nGone++
RETURN
