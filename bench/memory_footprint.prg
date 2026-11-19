// Memory footprint, read from the collector after full collections (__GcCollect(): the live objects; __GcMemory(): the
// heap's bytes, the free blocks it keeps for the next allocations included): an array of 1,000,000 integers, 200,000
// objects of a class of three variables, a hash of 300,000 string keys, 600,000 records { number, 11-character
// string }, each made by a function and measured before it returns. Each must stay within its count of live objects
// per element and a budget of heap bytes per element (a third or more above what it takes now: it catches growth, not
// the free blocks kept), and once they are all dropped the live objects and the heap must come back near where they
// started. The time is that of making them and of the collections.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nBase, nHeap, ok
   nBase := __GcCollect()
   nHeap := __GcMemory()[ 2 ]
   ok := Integers( nBase, nHeap ) .AND. Objects( nBase, nHeap ) .AND. Keys( nBase, nHeap ) .AND. ;
         Records( nBase, nHeap )
   ok := ok .AND. __GcCollect() <= nBase + 2 .AND. __GcMemory()[ 2 ] <= nHeap + 16 * 1024 * 1024
   ? "memory_footprint", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

// Whether the structure x, of n elements, is within nObjects live objects and nBytes heap bytes per element.
STATIC FUNCTION Within( x, n, nObjects, nBytes, nBase, nHeap )
   LOCAL nLive := __GcCollect() - nBase, nUsed := __GcMemory()[ 2 ] - nHeap
RETURN Len( x ) == n .AND. nLive <= n * nObjects + 4 .AND. nUsed <= n * nBytes

STATIC FUNCTION Integers( nBase, nHeap )
   LOCAL a := Array( 1000000 ), i
   FOR i := 1 TO Len( a )
      a[ i ] := i * 3
   NEXT
RETURN Within( a, 1000000, 0, 24, nBase, nHeap )

STATIC FUNCTION Objects( nBase, nHeap )
   LOCAL a := Array( 200000 ), i
   FOR i := 1 TO Len( a )
      a[ i ] := TPoint():New( i, -i )
   NEXT
RETURN Within( a, 200000, 1, 128, nBase, nHeap )

STATIC FUNCTION Keys( nBase, nHeap )
   LOCAL h := { => }, i
   FOR i := 1 TO 300000
      h[ "k" + Str( i, 7 ) ] := i
   NEXT
RETURN Within( h, 300000, 1, 224, nBase, nHeap )

STATIC FUNCTION Records( nBase, nHeap )
   LOCAL a := Array( 600000 ), i
   FOR i := 1 TO Len( a )
      a[ i ] := { i, "item" + Str( i, 7 ) }
   NEXT
RETURN Within( a, 600000, 2, 224, nBase, nHeap )

CLASS TPoint
   VAR x INIT 0
   VAR y INIT 0
   VAR cTag INIT "point"
   METHOD New( x, y ) INLINE ::x := x, ::y := y, Self
ENDCLASS
