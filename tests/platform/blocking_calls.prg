// For platform.blocking_calls: tasks on one worker beside calls of BlockFor( n ), which the test's C side runs as the
// runtime runs a file's I/O (an operation that usually returns at once) and which sleeps n milliseconds, as a read
// that waits would, or (n below 0) works -n microseconds, as a short read does.

// A task in a call that blocks 400 ms, then eight tasks queued behind it: { the most milliseconds the eight took to
// end from the start, the blocked one's }. Each is waited for with a limit (a wait without one would run the task on
// this thread).
FUNCTION BlockedCall()
   LOCAL nStart := ElapsedMs(), oLong, a := {}, k, nQuick := 0
   oLong := TaskRun( {|| BlockFor( 400 ), ElapsedMs() - nStart } )
   FOR k := 1 TO 8
      AAdd( a, TaskRun( {|| ElapsedMs() - nStart } ) )
   NEXT
   FOR k := 1 TO 8
      a[ k ]:Wait( 100000 )
      nQuick := Max( nQuick, a[ k ]:Result )
   NEXT
   oLong:Wait( 100000 )
RETURN { nQuick, oLong:Result }

// 32 tasks that each make 4000 calls that return soon, queued all along, while this thread adds timers, each due
// before those added already (the monitor, which serves the timers, wakes for each one and looks at the workers far
// more often than every 50 microseconds): the workers alive after.
FUNCTION ShortCalls()
   LOCAL a := {}, k, nDue := 600000
   FOR k := 1 TO 32
      AAdd( a, TaskRun( {|| Calls( 4000 ) } ) )
   NEXT
   DO WHILE !a[ 32 ]:IsCompleted .AND. nDue > 1000
      TaskDelay( nDue-- )
   ENDDO
   FOR k := 1 TO 32
      a[ k ]:Wait( 100000 )
   NEXT
RETURN TaskWorkers()[ 1 ]

STATIC FUNCTION Calls( n )
   LOCAL k
   FOR k := 1 TO n
      BlockFor( -5 )
   NEXT
RETURN n
