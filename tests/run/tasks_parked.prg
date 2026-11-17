// Suspended asynchronous functions across many collections: 2000 of them, each with a PRIVATE of its own, a SET
// DECIMALS of its own and a string only its environment holds, wait while young and full collections run (their
// environments move to the lists only full collections scan); resumed, each finds its values intact. Functions that
// opened a table shared and were abandoned (their tasks unreachable) have their tables closed after the collection
// that finds them, before any task runs a step: the table then opens exclusively.
PROCEDURE Main()
   LOCAL aSrc := {}, aT := {}, i, a, nBad := 0
   FOR i := 1 TO 2000
      AAdd( aSrc, TaskCompletionSource() )
      AAdd( aT, Waits( aSrc[ i ], i ) )
   NEXT
   FOR i := 1 TO 3000000
      a := { i, Str( i ) }
      IF i % 500000 == 0
         __GcCollect( i % 1000000 != 0 )
      ENDIF
   NEXT
   FOR i := 1 TO Len( aSrc )
      aSrc[ i ]:SetResult( i * 3 )
   NEXT
   FOR i := 1 TO Len( aT )
      IF ! aT[ i ]:Result == Expected( i )
         nBad++
      ENDIF
   NEXT
   ? "suspended across collections:", IIf( nBad == 0, "all intact", LTrim( Str( nBad ) ) + " WRONG" )

   dbCreate( "tasks_parked", { { "N", "N", 5, 0 } } )
   FOR i := 1 TO 300
      Opens( TaskCompletionSource() )
   NEXT
   __GcCollect()
   FOR i := 1 TO 200000
      a := { i, Str( i ) }
   NEXT
   USE tasks_parked NEW EXCLUSIVE
   ? "abandoned tables closed:", ! NetErr() .AND. Used()
   CLOSE
   FErase( "tasks_parked.dbf" )
RETURN

STATIC FUNCTION Expected( i )
RETURN "v" + LTrim( Str( i ) ) + "/" + Str( i % 7, 1, 0 ) + "/" + LTrim( Str( i * 3 ) ) + "/" + Replicate( "s", i % 50 )

ASYNC FUNCTION Waits( src, i )
   LOCAL x, c := Replicate( "s", i % 50 )
   PRIVATE pMine := "v" + LTrim( Str( i ) )
   SET DECIMALS TO i % 7
   x := AWAIT src:Task
RETURN Mine() + "/" + Str( Set( _SET_DECIMALS ), 1, 0 ) + "/" + LTrim( Str( x ) ) + "/" + c

STATIC FUNCTION Mine()
   MEMVAR pMine
RETURN pMine

ASYNC FUNCTION Opens( src )
   LOCAL x
   USE tasks_parked NEW SHARED
   x := AWAIT src:Task
RETURN x
