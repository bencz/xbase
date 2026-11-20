// One worker (//WORKERS:1 in tasks_next_chain.args): a task that a task starts runs next on its worker, for locality,
// and the task that was to run next goes to the worker's queue. A chain of such tasks, each starting the next one
// and ending, shares one slice of time, after which a task waiting elsewhere runs: one in the worker's own queue (the
// task started before the chain, pushed there by the chain's first task), one that the program's thread submits while
// the chain runs (its own queue, which the worker reaches as it steals), and the continuation of a delay, which the
// timers' thread submits. Each sets a flag that ends the chain long before its last link (one starved, it would run
// only after LINKS of them). Two chains on the one worker take turns: each ends once the other made 1000 links or
// ended, long before LINKS.
#define LINKS 3000000

PROCEDURE Main()
   LOCAL oDone := TaskCompletionSource(), aFlag := { .F. }, nStart

   TaskRun( {|| Start( oDone ) } ):Wait( 600000 )
   oDone:Task:Wait( 600000 )
   ? "chain stopped by the worker's queued task:", oDone:Task:Result < LINKS

   oDone := TaskCompletionSource()
   TaskRun( {|| Link( aFlag, 1, oDone ) } )
   nStart := ElapsedMs()
   DO WHILE ElapsedMs() - nStart < 50 .AND. !oDone:Task:IsCompleted // the chain runs meanwhile
   ENDDO
   TaskRun( {|| aFlag[ 1 ] := .T. } )
   oDone:Task:Wait( 600000 )
   ? "chain stopped by the program thread's task:", oDone:Task:Result < LINKS

   oDone := TaskCompletionSource()
   aFlag := { .F. }
   TaskDelay( 5 ):ContinueWith( {|| aFlag[ 1 ] := .T. } )
   TaskRun( {|| Link( aFlag, 1, oDone ) } )
   oDone:Task:Wait( 600000 )
   ? "chain stopped by a timer's continuation:", oDone:Task:Result < LINKS

   ? "two chains take turns:", TwoChains()
RETURN

STATIC FUNCTION TwoChains()
   LOCAL aCount := { 0, 0 }, aDone := { TaskCompletionSource(), TaskCompletionSource() }
   TaskRun( {|| Turns( aCount, 1, aDone ) } )
   TaskRun( {|| Turns( aCount, 2, aDone ) } )
   aDone[ 1 ]:Task:Wait( 600000 )
   aDone[ 2 ]:Task:Wait( 600000 )
RETURN aDone[ 1 ]:Task:Result < LINKS .AND. aDone[ 2 ]:Task:Result < LINKS

// A link of chain i: its count goes up; it ends once the other chain made 1000 links or ended (or it made LINKS).
STATIC PROCEDURE Turns( aCount, i, aDone )
   ++aCount[ i ]
   IF aCount[ 3 - i ] >= 1000 .OR. aDone[ 3 - i ]:Task:IsCompleted .OR. aCount[ i ] >= LINKS
      aDone[ i ]:SetResult( aCount[ i ] )
   ELSE
      TaskRun( {|| Turns( aCount, i, aDone ) } )
   ENDIF
RETURN

STATIC PROCEDURE Start( oDone )
   LOCAL aFlag := { .F. }
   TaskRun( {|| aFlag[ 1 ] := .T. } ) // next on this worker...
   TaskRun( {|| Link( aFlag, 1, oDone ) } ) // ...till the chain takes its place: queued
RETURN

STATIC PROCEDURE Link( aFlag, n, oDone )
   IF aFlag[ 1 ] .OR. n >= LINKS
      oDone:SetResult( n )
   ELSE
      TaskRun( {|| Link( aFlag, n + 1, oDone ) } )
   ENDIF
RETURN
