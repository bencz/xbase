// Timers added from several threads at once go to heaps of their own (shards, one a thread), and still fire in the
// order of their deadlines, all of them, none early. With one worker (//WORKERS:1 in tasks_timers_shards.args) the
// delays' continuations run in the order their timers fired: 8 threads, which start adding together, each add 40
// delays whose times interleave with the other threads' (50 ms apart, so that the threads' small skew cannot reorder
// them), and each delay's continuation records its time: the times come in order, 320 of them, each one at least
// its milliseconds after the start.
#define THREADS 8
#define EACH 40

PROCEDURE Main()
   LOCAL aThreads := {}, k, aFlag := { .F. }, aOrder := {}, aAll := {}, nStart, nEarly := 0, nOut := 0
   FOR k := 1 TO THREADS
      AAdd( aThreads, ThreadStart( {| n | Adds( n, aFlag, aOrder ) }, k ) )
   NEXT
   nStart := ElapsedMs()
   aFlag[ 1 ] := .T.
   AEval( aThreads, {| o | AEval( o:Join(), {| x | AAdd( aAll, x ) } ) } )
   TaskWhenAll( aAll ):Wait( 60000 )
   FOR k := 2 TO Len( aOrder )
      IF aOrder[ k ][ 1 ] < aOrder[ k - 1 ][ 1 ]
         ++nOut
      ENDIF
   NEXT
   AEval( aOrder, {| a | IIf( a[ 2 ] - nStart < a[ 1 ] - 5, ++nEarly, NIL ) } )
   ? "fired:", Len( aOrder ), "out of order:", nOut, "early:", nEarly
RETURN

// Adds EACH delays once the flag is set: their continuations' tasks.
STATIC FUNCTION Adds( n, aFlag, aOrder )
   LOCAL a := {}, k, nMs
   DO WHILE !aFlag[ 1 ]
   ENDDO
   FOR k := 1 TO EACH
      nMs := 50 * ( ( k * 7 + n * 3 ) % 12 ) + 50
      AAdd( a, Recorded( TaskDelay( nMs ), nMs, aOrder ) )
   NEXT
RETURN a

STATIC FUNCTION Recorded( oDelay, nMs, aOrder )
RETURN oDelay:ContinueWith( {|| AAdd( aOrder, { nMs, ElapsedMs() } ) } )
