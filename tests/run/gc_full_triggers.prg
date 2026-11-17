// Old objects that die are found by a full collection even when the old generation stops growing: a large old
// structure with an object that has a destructor is dropped, then only short-lived objects are made (nothing becomes
// old): a full collection comes once eight times the live data was allocated, and the destructor runs. Destructors run
// while the program's thread waits in a Join(): the thread that allocates runs them (the program's thread would never
// test anything while it waits), one context at a time; every one runs once.
STATIC s_nGone := 0, s_a

PROCEDURE Main()
   LOCAL i, n, t
   s_a := Array( 200000 )
   FOR i := 1 TO Len( s_a )
      s_a[ i ] := { i, Space( 100 + i % 7 ) }
   NEXT
   s_a[ 1 ] := Node():New()
   __GcCollect() // all old now, the live data counted
   s_a := NIL
   FOR i := 1 TO 100000000
      Keep( { i, Str( i ) } )
      IF s_nGone > 0
         EXIT
      ENDIF
   NEXT
   ? "destructor of old garbage ran before 100M allocations:", s_nGone == 1, i < 100000000

   s_nGone := 0
   t := ThreadStart( {|| Churn( 200000 ) } )
   n := t:Join()
   ? "destructors ran while the program's thread joined:", n > 100000
   __GcCollect()
   __GcCollect()
   ? "each once:", s_nGone == 200000
RETURN

STATIC FUNCTION Keep( x )
RETURN x

STATIC FUNCTION Churn( n )
   LOCAL i, o
   FOR i := 1 TO n
      o := Node():New()
      o:pad := Space( 200 + i % 5 )
   NEXT
RETURN s_nGone

CREATE CLASS Node
   VAR pad
   METHOD New INLINE Self
   DESTRUCTOR Bye
ENDCLASS

METHOD PROCEDURE Bye() CLASS Node
   s_nGone++
RETURN
