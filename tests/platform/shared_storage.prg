// For platform.shared_storage: the program's storage once a second thread runs (docs/concurrency.md, publication).
// Writers store alternately integers and doubles (whole items: a torn one is an integer of a double's bits or a tiny
// double) into a STATIC, through a codeblock that captures another STATIC (moved into a cell while the threads run),
// into a class variable, and into an element of an array a STATIC held before the threads started; readers read them
// back the same ways and count what is not a value some writer stored. Also: a STATIC passed by reference, a STATIC as
// a FOR EACH variable (and one STATIC both readers loop over at once: the program's race, which ends no loop badly), a
// class prepared before the threads, and an array a STATIC holds that the first thread grows while the others attach
// (the program's storage becomes shared during its allocations).
STATIC sValue, sCaptured, sArray, sRef, sLoop1, sLoop2, sLoop, sGrow

// The STATICs' values before the threads start (the test runs no program, so no module initialization runs).
FUNCTION StorageStart()
   sValue := 0
   sCaptured := 0
   sRef := 0
   sArray := AFill( Array( 8 ), 0 )
   sGrow := {}
   TStore():New():nShared := 0 // the class prepared now
RETURN sArray

// Appends to the array sGrow holds, while the others attach and read it.
FUNCTION StorageGrow( n )
   LOCAL k
   FOR k := 1 TO n
      AAdd( sGrow, k % 1000 + 0.5 * ( k % 2 ) )
   NEXT
RETURN Len( sGrow )

// Each round: a burst of stores of one value (the writers' values are of opposite types at the same time) into each
// kind of storage.
FUNCTION StorageWriter( nRounds, nMe )
   LOCAL n, k, x, o := TStore():New(), b
   FOR n := 1 TO nRounds
      x := iif( ( Int( n / 8 ) + nMe ) % 2 == 0, n, n + 0.5 )
      FOR k := 1 TO 32
         sValue := x
      NEXT
      IF n % 64 == nMe
         b := {| v | sCaptured := v } // the STATIC moves into a cell, once, while others read and write it
      ENDIF
      FOR k := 1 TO 32
         IF b == NIL
            sCaptured := x
         ELSE
            Eval( b, x )
         ENDIF
      NEXT
      FOR k := 1 TO 32
         o:nShared := x
      NEXT
      FOR k := 1 TO 8
         Put( @sRef, x )
      NEXT
      FOR k := 1 TO 32
         sArray[ k % 8 + 1 ] := x
      NEXT
   NEXT
RETURN NIL

FUNCTION StorageReader( nRounds, nMe )
   LOCAL n, k, o := TStore():New(), nBad := 0, b := {|| sCaptured }
   FOR n := 1 TO nRounds
      FOR k := 1 TO 32
         IF !Valid( sValue )
            ++nBad
         ENDIF
      NEXT
      FOR k := 1 TO 32
         IF !Valid( sCaptured ) .OR. !Valid( Eval( b ) )
            ++nBad
         ENDIF
      NEXT
      FOR k := 1 TO 32
         IF !Valid( o:nShared )
            ++nBad
         ENDIF
      NEXT
      FOR k := 1 TO Len( sArray )
         IF !Valid( sArray[ k ] )
            ++nBad
         ENDIF
      NEXT
      IF nMe == 0 // a STATIC as the loop variable (each reader its own): elements read through its cell
         FOR EACH sLoop1 IN sArray
            IF !Valid( sLoop1 )
               ++nBad
            ENDIF
         NEXT
      ELSE
         FOR EACH sLoop2 IN sArray
            IF !Valid( sLoop2 )
               ++nBad
            ENDIF
         NEXT
      ENDIF
      FOR EACH sLoop IN sArray // both readers at once: what it holds is either loop's, or what one left
      NEXT
      IF !Valid( Get( @sRef ) )
         ++nBad
      ENDIF
      FOR k := Max( 1, Len( sGrow ) - 16 ) TO Len( sGrow )
         IF !Valid( sGrow[ k ] )
            ++nBad
         ENDIF
      NEXT
      IF n % 1024 == 0
         __GcCollect( n % 4096 != 0 )
      ENDIF
   NEXT
RETURN nBad

STATIC FUNCTION Valid( x )
RETURN ValType( x ) == "N" .AND. x >= 0 .AND. x < 4294967296 .AND. ( x == Int( x ) .OR. x - Int( x ) == 0.5 )

CLASS TStore
   CLASSVAR nShared INIT 0
ENDCLASS

STATIC FUNCTION Put( r, x )
   r := x
RETURN NIL

STATIC FUNCTION Get( r )
RETURN r
