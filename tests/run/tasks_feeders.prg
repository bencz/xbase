// Tasks submitted by threads that are no workers (the program's thread, ThreadStart()'s): each such thread fills a
// queue of its own that the workers take from. A thread that submits 500 tasks and ends at once (its queue goes to
// the pool's: every task runs); 16 threads submitting 200 tasks each at the same time; 40 threads one after another
// (the queue of an ended thread serves the next one), each submitting 10; the program's thread submitting while its
// own tasks submit more (from workers). Every task runs once: the sums tell.
PROCEDURE Main()
   LOCAL oAll := AtomicInt(), aTasks := {}, oT, a := {}, k, n, t

   oT := ThreadStart( {|| Submit( aTasks, 500, oAll ) } )
   oT:Join()
   ? "ended thread:", TaskWhenAll( aTasks ):Wait( 30000 ), Len( aTasks ), oAll:Value

   oAll := AtomicInt()
   aTasks := Array( 16 )
   FOR k := 1 TO 16
      AAdd( a, ThreadStart( {| n | aTasks[ n ] := {}, Submit( aTasks[ n ], 200, oAll ) }, k ) )
   NEXT
   AEval( a, {| x | x:Join() } )
   n := 0
   FOR k := 1 TO 16
      n += iif( TaskWhenAll( aTasks[ k ] ):Wait( 30000 ), Len( aTasks[ k ] ), 0 )
   NEXT
   ? "at once:", n, oAll:Value

   oAll := AtomicInt()
   aTasks := {}
   FOR k := 1 TO 40
      ThreadStart( {|| Submit( aTasks, 10, oAll ) } ):Join()
   NEXT
   ? "one after another:", TaskWhenAll( aTasks ):Wait( 30000 ), Len( aTasks ), oAll:Value

   oAll := AtomicInt()
   aTasks := {}
   FOR k := 1 TO 300
      AAdd( aTasks, TaskRun( {|| TaskRun( {|| oAll:Add( 1 ) } ):Wait( 30000 ), oAll:Add( 1 ) } ) )
   NEXT
   t := TaskWhenAll( aTasks )
   ? "nested:", t:Wait( 30000 ), oAll:Value
RETURN

// Submits n tasks that each add 1, the tasks kept in a (only this thread adds to it while it runs).
STATIC PROCEDURE Submit( a, n, oAll )
   LOCAL k
   FOR k := 1 TO n
      AAdd( a, TaskRun( {|| oAll:Add( 1 ) } ) )
   NEXT
RETURN
