// Objects that only dead values held are collected (their destructors run): a chain built in a loop by an inlined
// function, then dropped; objects made in a loop, the last one dropped after it; values of a loop nested in another.
// A value's frame slot is cleared where its loop ends, so that what it held is not kept until the slot is reused.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL k := NIL, i, j, o
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
