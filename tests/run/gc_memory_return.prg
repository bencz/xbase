// Memory goes back to the system after a peak: the heap's memory (__GcMemory()[ 2 ], blocks in use and large objects'
// units and mappings) after a full collection comes back near where it started, for each kind of object: small ones
// (lines of blocks), those of size classes (2 KB strings), large ones in units of chunks (10 KB strings, all alive,
// then one in a hundred kept: their chunks hold a few each), large ones with mappings of their own (2 MB strings); a
// size class whose objects all lived, then died but one in 64 (each block left with one: they are emptied into the
// fullest ones); a long array, whose items are traced a slice at a time. What is kept is intact after each collection.
// The bounds are generous (a few MB of blocks and units kept for the next allocations), the peaks far above them.
PROCEDURE Main()
   LOCAL a, i, m, nBase, lOk
   __GcCollect()
   nBase := Heap()
   a := Array( 300000 )
   FOR i := 1 TO Len( a )
      a[ i ] := { i, "x" + LTrim( Str( i ) ) }
   NEXT
   ? "small:", Heap() > nBase + 30, Back( @a, nBase )

   a := Array( 30000 )
   FOR i := 1 TO Len( a )
      a[ i ] := Replicate( "c", 2000 + i % 3 )
   NEXT
   ? "size classes:", Heap() > nBase + 50, Back( @a, nBase )

   m := Array( 20000 ) // all alive first: then the kept ones lie a few in each chunk
   FOR i := 1 TO Len( m )
      m[ i ] := Replicate( "L", 10000 + i % 3 )
   NEXT
   a := Keep( @m, 100 )
   __GcCollect()
   lOk := .T.
   AEval( a, {| c, k | lOk := lOk .AND. Len( c ) == 10000 + ( k * 100 ) % 3 .AND. Right( c, 1 ) == "L" } )
   ? "large units, one in a hundred kept:", Len( a ), lOk, Heap() < nBase + 24, Back( @a, nBase )

   a := {}
   FOR i := 1 TO 40
      AAdd( a, Replicate( "M", 2000000 + i ) )
   NEXT
   ? "large mappings:", Heap() > nBase + 70, Back( @a, nBase )

   m := Array( 200000 ) // all alive first: then the kept ones lie one in each block
   FOR i := 1 TO Len( m )
      m[ i ] := Replicate( "f", 400 + i % 13 )
   NEXT
   a := Keep( @m, 64 )
   __GcCollect()
   __GcCollect()
   lOk := .T.
   AEval( a, {| c, k | lOk := lOk .AND. Len( c ) == 400 + ( k * 64 ) % 13 .AND. Left( c, 1 ) == "f" } )
   ? "size class, one in 64 kept:", Len( a ), lOk, Heap() < nBase + 24, Back( @a, nBase )

   a := Array( 1000000 )
   FOR i := 1 TO Len( a )
      a[ i ] := { i }
   NEXT
   __GcCollect()
   lOk := .T.
   FOR i := 1 TO Len( a ) STEP 997
      lOk := lOk .AND. a[ i ][ 1 ] == i
   NEXT
   ? "long array:", lOk, Back( @a, nBase )
RETURN

// Every n-th element of `a`, which is dropped.
STATIC FUNCTION Keep( a, n )
   LOCAL k := {}, i
   FOR i := n TO Len( a ) STEP n
      AAdd( k, a[ i ] )
   NEXT
   a := NIL
RETURN k

// The heap's memory, in MB.
STATIC FUNCTION Heap()
RETURN __GcMemory()[ 2 ] / 1048576

// Whether the memory went back, near the start, once `a` is dropped and collected.
STATIC FUNCTION Back( a, nBase )
   a := NIL
   __GcCollect()
   __GcCollect()
RETURN Heap() < nBase + 24
