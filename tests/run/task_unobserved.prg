// A failed task that nothing observed is reported once the collector finds it unreachable: to the standard error
// without a handler (task_unobserved.err), to the block TaskUnobservedBlock() set with one, which gets the failure and
// the task. Observing is waiting for the task (Wait(), Result, AWAIT), continuing from it (ContinueWith(), the
// combinators, TaskTimeout()) or reading its failure (Error, IsFailed); IsCompleted and IsCanceled are not. A task that
// completed or was canceled is never reported; neither is one still reachable. An ASYNC function called without AWAIT
// whose error fails it is reported too; the body of an asynchronous generator, whose enumerator takes its failure,
// is not.
#include "error.ch"

STATIC s_aSeen := {}

PROCEDURE Main()
   LOCAL oKept, oSource, o, aKind, bOld

   Dropped( {|| 1 + "a" } ) // no handler yet: reported to the standard error
   Collect()
   ? "handler before:", ValType( TaskUnobservedBlock() )

   bOld := TaskUnobservedBlock( {| x, oTask | AAdd( s_aSeen, Describe( x, oTask ) ) } )
   ? "previous:", ValType( bOld ), "now:", ValType( TaskUnobservedBlock() )
   TaskUnobservedBlock( "not a block" ) // ignored, as ErrorBlock() ignores it
   ? "still a block:", ValType( TaskUnobservedBlock() )

   Dropped( {|| 1 + "b" } )
   Dropped( {|| Break( "broken" ) } )
   Collect()
   Report( "unobserved" )

   oKept := Failing( {|| 1 + "c" } ) // reachable: not reported
   Collect()
   Report( "kept" )
   ? "kept failed:", oKept:IsFailed
   oKept := NIL
   Collect()
   Report( "observed by IsFailed" )

   FOR EACH aKind IN { "Error", "IsFailed", "Wait", "Result", "ContinueWith", "WhenAll", "WhenAny", "Timeout", ;
         "Await", "IsCompleted", "IsCanceled" }
      Observed( aKind )
      Collect()
      Report( "after " + aKind )
   NEXT

   oSource := TaskCompletionSource()
   oSource:SetResult( 1 )
   oSource := TaskCompletionSource()
   oSource:SetCanceled()
   o := TaskRun( {|| 42 } )
   DO WHILE !o:IsCompleted
      TaskYield():Wait()
   ENDDO
   oSource := o := NIL
   Collect()
   Report( "completed and canceled" )

   AsyncDropped() // its task, never awaited, fails
   Collect()
   Report( "async function" )

   GeneratorDropped() // its body fails after a suspension: its enumerator takes the failure, observed there
   Collect()
   Report( "async generator" )
RETURN

// A task that runs bWork and fails, once it has ended (IsCompleted observes nothing).
STATIC FUNCTION Failing( bWork )
   LOCAL oTask := TaskRun( {|| Eval( bWork ) } )
   DO WHILE !oTask:IsCompleted
      TaskYield():Wait()
   ENDDO
RETURN oTask

// A task that fails, ended and dropped: in a frame of its own, which returning ends (no temporary of the caller's
// frame holds it, -O0 too).
STATIC PROCEDURE Dropped( bWork )
   Failing( bWork )
RETURN

// A failed task observed (or not) the way cKind names.
STATIC PROCEDURE Observed( cKind )
   LOCAL oTask := Failing( {|| 1 + "d" } ), oOther, lFailed
   DO CASE
   CASE cKind == "Error"
      oOther := oTask:Error
   CASE cKind == "IsFailed"
      oOther := oTask:IsFailed
   CASE cKind == "Wait"
      oOther := oTask:Wait()
   CASE cKind == "Result"
      BEGIN SEQUENCE WITH {| oErr | Break( oErr ) }
         oOther := oTask:Result
      END SEQUENCE
   CASE cKind == "ContinueWith"
      oOther := oTask:ContinueWith( {|| NIL } )
      oOther:Wait()
   CASE cKind == "WhenAll"
      oOther := TaskWhenAll( { oTask } )
      oOther:Wait()
      lFailed := oOther:IsFailed // the combinator's failure observed too
   CASE cKind == "WhenAny"
      oOther := TaskWhenAny( { oTask } )
      oOther:Wait()
   CASE cKind == "Timeout"
      oOther := TaskTimeout( oTask, 10000 )
      oOther:Wait()
      lFailed := oOther:IsFailed
   CASE cKind == "Await"
      oOther := Awaits( oTask )
      oOther:Wait()
      lFailed := oOther:IsFailed
   CASE cKind == "IsCompleted"
      oOther := oTask:IsCompleted
   CASE cKind == "IsCanceled"
      oOther := oTask:IsCanceled
   ENDCASE
RETURN

STATIC ASYNC FUNCTION Awaits( oTask )
RETURN AWAIT oTask

STATIC PROCEDURE AsyncDropped()
   LOCAL o := FailsAsync()
   DO WHILE !o:IsCompleted
      TaskYield():Wait()
   ENDDO
RETURN

STATIC PROCEDURE GeneratorDropped()
   LOCAL g := FailingGenerator(), x
   BEGIN SEQUENCE WITH {| oErr | Break( oErr ) }
      x := g:MoveNextAsync():Result
      x := g:MoveNextAsync():Result
   END SEQUENCE
RETURN

STATIC ASYNC FUNCTION FailingGenerator()
   YIELD 1
   AWAIT TaskYield()
   YIELD 1 + "f"
RETURN

STATIC ASYNC FUNCTION FailsAsync()
   AWAIT TaskYield()
RETURN 1 + "e"

STATIC FUNCTION Describe( x, oTask )
   IF ValType( x ) == "O"
      RETURN x:description + " " + x:operation + " " + IIf( oTask:IsFailed, "failed", "?" )
   ENDIF
RETURN ValType( x ) + " " + x + " " + IIf( oTask:IsFailed, "failed", "?" )

// Full collections: what they find unreachable is finalized (reported) before __GcCollect() returns.
STATIC PROCEDURE Collect()
   __GcCollect()
   __GcCollect()
RETURN

STATIC PROCEDURE Report( cWhen )
   ASort( s_aSeen )
   ? cWhen + ":", Len( s_aSeen )
   AEval( s_aSeen, {| c | QQOut( " [" + c + "]" ) } )
   s_aSeen := {}
RETURN
