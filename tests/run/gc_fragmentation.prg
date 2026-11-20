// Full collections compact strings left among dead ones: 120,000 strings of 100 bytes, each made by nine `+` whose
// results die (the kept one lies at a random offset among them, straddling two lines most of the time), take after a
// few full collections no more of the heap's blocks (__GcMemory()[ 4 ], without the free ones kept for allocations)
// than the same strings made one after another by Replicate(), give or take a tenth: a block is emptied by how many of
// its bytes live, not by how many of its lines are in use. The same for strings made by appends to an element. Every
// string is intact after each collection.
#define COUNT 120000

PROCEDURE Main()
   LOCAL c := Left( "abcdefghij" + Str( Seconds() ), 10 ), a, nPlain, nCat, nAppend

   a := Built( c, "plain" )
   nPlain := Settled( a, c )
   a := NIL
   __GcCollect()

   a := Built( c, "cat" )
   nCat := Settled( a, c )
   a := NIL
   __GcCollect()

   a := Built( c, "append" )
   nAppend := Settled( a, c )
   ? "intact:", nPlain > 0, nCat > 0, nAppend > 0
   ? "compacted:", nCat <= nPlain * 1.1, nAppend <= nPlain * 1.1
RETURN

STATIC FUNCTION Built( c, cMode )
   LOCAL a := Array( COUNT ), i, k
   FOR i := 1 TO COUNT
      IF cMode == "plain"
         a[ i ] := Replicate( c, 10 )
      ELSEIF cMode == "cat"
         a[ i ] := c + c + c + c + c + c + c + c + c + c
      ELSE
         a[ i ] := ""
         FOR k := 1 TO 10
            a[ i ] += c
         NEXT
      ENDIF
   NEXT
RETURN a

// The heap's occupied blocks after four full collections, -1 when a string changed.
STATIC FUNCTION Settled( a, c )
   LOCAL i, cAll := Replicate( c, 10 ), nHeap
   FOR i := 1 TO 4
      __GcCollect()
   NEXT
   nHeap := __GcMemory()[ 4 ]
   FOR i := 1 TO Len( a )
      IF !( a[ i ] == cAll )
         RETURN -1
      ENDIF
   NEXT
RETURN nHeap
