// Objects that only dead values held are collected (their destructors run): a chain built in a loop by an inlined
// function, then dropped; objects made in a loop, the last one dropped after it; values of a loop nested in another;
// values of loops that flow into another loop, a FOR and a DO WHILE left by EXIT; values of a loop left by BREAK into
// the RECOVER of a sequence around it (its unwind edge); the collection of a FOR EACH, made before the loop and dead
// after it, in a loop around that makes the next one (its slot is cleared where the FOR EACH ends, as the next one is
// being made). A value's frame slot is cleared where its loop ends, so that what it held is not kept
// until the slot is reused.
STATIC s_nGone := 0
STATIC s_cSeen := ""

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
   ? "for each source", ForEachSource(), s_cSeen
RETURN

// A collection made in a loop and gone through by a FOR EACH (the optimized code tests its type before the loop too):
// the last turn's is dead when the next one is being made.
STATIC FUNCTION ForEachSource()
   LOCAL i, o, n := 0
   s_nGone := 0
   FOR i := 1 TO 3
      FOR EACH o IN Collected( i )
         n += o:n
      NEXT
   NEXT
RETURN n

// Makes three nodes after a full collection, which finds the last call's dead (their count shown).
STATIC FUNCTION Collected( i )
   __GcCollect()
   s_cSeen += Str( s_nGone, 3 )
RETURN { Node():New( i ), Node():New( i * 10 ), Node():New( i * 100 ) }

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
