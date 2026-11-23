// Full collections tell their marks from the last one's by a polarity each one turns around, clearing none first. Over
// many full collections in a row (each polarity many times): objects that stay alive keep their contents (small ones
// in lines, arrays of a size class's slots, large strings and arrays); objects that were old through several full
// collections and then die are collected by the next one (their destructors run then, once), whatever polarity they
// last had; dead objects that share lines with live ones never come back; objects made between full collections, and
// those that only young collections saw, are kept while reachable.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL aKeep := {}, aSmall := {}, aClass := {}, cLarge := Replicate( "L", 300000 ), aLarge := Array( 70000 ), i, k
   LOCAL aDying := {}, nBad := 0
   AFill( aLarge, 7 )
   FOR i := 1 TO 2000
      AAdd( aSmall, { i, Str( i ) } )                 // small, in lines; every other one dies below
      AAdd( aClass, Array( 40 ) )                      // a size class's slots
      AFill( ATail( aClass ), i )
   NEXT
   FOR i := 1 TO 30
      AAdd( aDying, Node():New( i ) )                 // old through several full collections, then dropped
   NEXT
   FOR k := 1 TO 9                                     // nine full collections: each polarity several times
      FOR i := 2 TO Len( aSmall ) STEP 2
         IF k == 3
            aSmall[ i ] := NIL                         // dead neighbours of live objects in the same lines
         ENDIF
      NEXT
      IF k == 5
         ASize( aDying, 0 )                            // old since the first collections: the next full finds them dead
      ENDIF
      FOR i := 1 TO 300
         AAdd( aKeep, { k, i, Replicate( Chr( 64 + k ), 20 ) } ) // made between full collections
      NEXT
      __GcCollect( .T. )                               // a young one first: what it saw stays young or ages
      __GcCollect()
      IF k == 5
         ? "after the drop:", s_nGone
      ENDIF
   NEXT
   FOR i := 1 TO Len( aSmall )
      IF i % 2 == 1 .AND. ( aSmall[ i ][ 1 ] != i .OR. !( aSmall[ i ][ 2 ] == Str( i ) ) )
         nBad++
      ENDIF
      IF aClass[ i ][ 40 ] != i .OR. aClass[ i ][ 1 ] != i
         nBad++
      ENDIF
   NEXT
   FOR i := 1 TO Len( aKeep )
      IF aKeep[ i ][ 2 ] != ( i - 1 ) % 300 + 1 .OR. ;
            !( aKeep[ i ][ 3 ] == Replicate( Chr( 64 + aKeep[ i ][ 1 ] ), 20 ) )
         nBad++
      ENDIF
   NEXT
   ? "kept:", nBad == 0, Len( aKeep ), Len( cLarge ), Right( cLarge, 3 ), aLarge[ 1 ], aLarge[ 70000 ]
   ? "destructors:", s_nGone
RETURN

CLASS Node
   VAR n
   METHOD New( n ) INLINE ::n := n, Self
   DESTRUCTOR Gone()
ENDCLASS

METHOD Gone() CLASS Node
   s_nGone++
RETURN NIL
