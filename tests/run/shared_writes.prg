// Writes to an array threads share. ASort of an array another thread writes meanwhile is one operation among those
// writes: a value stored while a sort runs is never lost (without a block, with one, a range of the array), the array
// ends sorted, and sorts without a block end in time while another thread writes the array without a pause (the copy
// each sort reads first does not wait for a pause of the writer). A thread that writes an element gets its turn while
// another thread takes the array's lock again and again for long operations. A sort whose block writes the array (so
// that no copy is ever the array still) ends, the elements it wrote keeping their values. AClone of an array a thread
// writes without a pause, while another thread collects, gives whole copies.
PROCEDURE Main()
   ? "no block:", Writes( {| a | ASort( a ) } )
   ? "block:", Writes( {| a | ASort( a, , , {| x, y | x < y } ) } )
   ? "range:", Writes( {| a | ASort( a, 1, 600 ) } )
   ? "hot writer:", HotWriter()
   ? "turns:", Turns()
   ? "self-writing block:", SelfWriting()
   ? "copies:", Copies()
RETURN

// Rounds that each store a new value, greater than all, at the first element while a thread sorts the array again and
// again (each round at another point of a sort: after a sort ends and a pause of its own length), and wait for two
// more sorts: then every value stored is in the array, and it is sorted.
STATIC FUNCTION Writes( bSort )
   LOCAL a := Array( 800 ), aState := { 0, .F. }, oT, r, n, nLost := 0, lSorted := .T., k
   FOR k := 1 TO Len( a )
      a[ k ] := ( k * 7919 ) % 1000
   NEXT
   oT := ThreadStart( {|| Sorter( a, aState, bSort ) } )
   FOR r := 1 TO 60
      n := aState[ 1 ]
      DO WHILE aState[ 1 ] == n
      ENDDO
      FOR k := 1 TO ( r * 7919 ) % 1500
      NEXT
      a[ 1 ] := 1000000 + r
      n := aState[ 1 ]
      DO WHILE aState[ 1 ] < n + 2
      ENDDO
   NEXT
   aState[ 2 ] := .T.
   oT:Join()
   FOR r := 1 TO 60
      IF AScan( a, 1000000 + r ) == 0
         nLost++
      ENDIF
   NEXT
   FOR k := 2 TO 600
      IF a[ k - 1 ] > a[ k ]
         lSorted := .F.
      ENDIF
   NEXT
RETURN "lost " + LTrim( Str( nLost ) ) + ", sorted " + iif( lSorted, "yes", "no" ) + ", length " + ;
   LTrim( Str( Len( a ) ) )

STATIC PROCEDURE Sorter( a, aState, bSort )
   DO WHILE ! aState[ 2 ]
      Eval( bSort, a )
      aState[ 1 ]++
   ENDDO
RETURN

// 50 sorts without a block while the main thread writes the array as fast as it can: they take a few hundredths of
// a second (seconds when a sort waits for the writer to pause).
STATIC FUNCTION HotWriter()
   LOCAL a := Array( 3000 ), aState := { 0 }, oT, i := 0, k, nStart := Seconds()
   FOR k := 1 TO Len( a )
      a[ k ] := ( k * 7919 ) % 5000
   NEXT
   oT := ThreadStart( {|| Sorts( a, aState ) } )
   DO WHILE aState[ 1 ] < 50
      a[ ( i % 3000 ) + 1 ] := i % 5000
      i++
   ENDDO
   oT:Join()
RETURN LTrim( Str( aState[ 1 ] ) ) + " sorts " + iif( Seconds() - nStart < 2, "in time", "too slow" )

STATIC PROCEDURE Sorts( a, aState )
   DO WHILE aState[ 1 ] < 50
      ASort( a )
      aState[ 1 ]++
   ENDDO
RETURN

// 100 writes of an element while the main thread inserts into the array (200000 elements) again and again: they take
// a few hundredths of a second (the inserting thread would keep the lock for seconds if it could take it back from a
// waiting thread each time it releases it).
STATIC FUNCTION Turns()
   LOCAL a := Array( 200000 ), aState := { 0, 0 }, oT, nStart
   AFill( a, 0 )
   oT := ThreadStart( {|| Writes100( a, aState ) } )
   nStart := Seconds()
   DO WHILE aState[ 1 ] < 100
      AIns( a, 2 )
      aState[ 2 ]++
   ENDDO
   oT:Join()
RETURN LTrim( Str( aState[ 1 ] ) ) + " writes " + iif( Seconds() - nStart < 2, "in time", "too slow" )

STATIC PROCEDURE Writes100( a, aState )
   LOCAL k
   DO WHILE aState[ 2 ] < 10
   ENDDO
   FOR k := 1 TO 100
      a[ 1 ] := k
      aState[ 1 ]++
   NEXT
RETURN

// ASort of a shared array with a block that stores into the range it sorts at each comparison, or appends to the
// array (a range to its end): it ends; the element the block wrote keeps the value written, the others come back
// sorted.
STATIC FUNCTION SelfWriting()
   LOCAL a := { 5, 3, 9, 1, 7, 2 }, b := { 4, 8, 6 }, n := 0, lSorted := .T., k
   ThreadStart( {|| Len( a ) + Len( b ) } ):Join() /* shared */
   ASort( a, , , {| x, y | a[ 6 ] := 100 + ++n, x < y } )
   FOR k := 2 TO 5
      IF a[ k - 1 ] > a[ k ]
         lSorted := .F.
      ENDIF
   NEXT
   ASort( b, , , {| x, y | AAdd( b, 0 ), x < y } )
RETURN "last " + iif( a[ 6 ] == 100 + n, "written", "lost" ) + ", rest sorted " + iif( lSorted, "yes", "no" ) + ;
   ", appended to " + iif( Len( b ) > 3, "yes", "no" )

// 300 copies of an array of 2000 numbers that a thread writes without a pause while another collects: each copy whole.
STATIC FUNCTION Copies()
   LOCAL a := Array( 2000 ), aState := { .F. }, oW, oG, c, k, nBad := 0
   AFill( a, 1 )
   oW := ThreadStart( {|| HotWrites( a, aState ) } )
   oG := ThreadStart( {|| Garbage( aState ) } )
   FOR k := 1 TO 300
      c := AClone( a )
      IF Len( c ) != 2000 .OR. AScan( c, {| x | ValType( x ) != "N" } ) != 0
         nBad++
      ENDIF
   NEXT
   aState[ 1 ] := .T.
   oW:Join()
   oG:Join()
RETURN LTrim( Str( nBad ) ) + " bad"

STATIC PROCEDURE HotWrites( a, aState )
   LOCAL i := 0
   DO WHILE ! aState[ 1 ]
      a[ ( i % 2000 ) + 1 ] := i
      i++
   ENDDO
RETURN

STATIC PROCEDURE Garbage( aState )
   LOCAL c
   DO WHILE ! aState[ 1 ]
      c := Space( 5000 )
   ENDDO
RETURN
