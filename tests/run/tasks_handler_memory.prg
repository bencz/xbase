// The memory a suspended ASYNC function costs, and what tasks that end before their timers keep:
// - 50000 handlers, each suspended on AWAIT TaskDelay( far, oToken ) (as a server's connections waiting for input),
//   hold its task, its frame, the delay's task (a small one: a delay keeps no block, environment or frame) and their
//   links to the token: the heap they occupy, per handler, stays within a budget (a small task for the delay saved 96
//   bytes of the 900 each took with every task the full size);
// - once the token is canceled they all end canceled, and once they are dropped their objects are gone: the delays'
//   timers, far from due, are dropped as dead (compacted once they are half of the timers), not kept to their time;
// - 50000 TaskTimeout()s of tasks that end at once, then dropped: their objects are gone too.
// Each phase runs in a function of its own (what its frame held goes when it returns).
#define HANDLERS 50000

PROCEDURE Main()
   LOCAL nLive := __GcCollect(), nBase := __GcMemory()[ 4 ], aPhase
   aPhase := Handlers( nBase )
   ? "per handler within budget:", aPhase[ 1 ] < 860, IIf( aPhase[ 1 ] < 860, "", Str( aPhase[ 1 ] ) )
   ? "all ended canceled:", aPhase[ 2 ] == HANDLERS
   __GcCollect()
   ? "handlers' objects gone:", Gone( nLive )
   ? "timeouts ended in time:", Timeouts()
   __GcCollect()
   ? "timeouts' objects gone:", Gone( nLive )
RETURN

STATIC FUNCTION Gone( nLive )
   LOCAL nNow := __GcCollect()
RETURN IIf( nNow - nLive < 100, "yes", Str( nNow - nLive ) + " kept" )

// The handlers made, measured, canceled and waited for: { heap bytes per handler, the ones that ended canceled }.
STATIC FUNCTION Handlers( nBase )
   LOCAL a := Array( HANDLERS ), k, nHeld, oSource := CancellationTokenSource(), nCanceled := 0
   FOR k := 1 TO HANDLERS
      a[ k ] := Handler( k, oSource:Token )
   NEXT
   __GcCollect()
   nHeld := ( __GcMemory()[ 4 ] - nBase ) / HANDLERS
   oSource:Cancel()
   TaskWhenAll( a ):Wait( 60000 )
   AEval( a, {| o | IIf( o:Result == -1, ++nCanceled, NIL ) } )
RETURN { nHeld, nCanceled }

STATIC ASYNC FUNCTION Handler( k, oToken )
   LOCAL x := k
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AWAIT TaskDelay( 600000, oToken )
   RECOVER
      x := -1
   END SEQUENCE
RETURN x

// TaskTimeout( 10 minutes ) of tasks that end at once: each ends as its task did.
STATIC FUNCTION Timeouts()
   LOCAL a := Array( HANDLERS ), k, nOk := 0
   FOR k := 1 TO HANDLERS
      a[ k ] := TaskTimeout( TaskRun( {|| 1 } ), 600000 )
   NEXT
   TaskWhenAll( a ):Wait( 60000 )
   AEval( a, {| o | IIf( o:IsCompleted .AND. o:Result == 1, ++nOk, NIL ) } )
RETURN nOk == HANDLERS
