// Tasks under load, checked exactly: 20000 tasks fanned out and joined (TaskWhenAll keeps their order); a tree of
// TaskWhenAll 12 levels deep over 4096 leaves; 5000 delays of 1 to 50 ms whose continuations all run once and not
// before their time; 5000 sources whose delays and Register callbacks are canceled half by Cancel() and half by
// CancelAfter(); 1000 TaskWhenAny races that a delay wins; 2000 failures of both kinds (errors and BREAK values)
// counted, and the first failure in order is the one TaskWhenAll keeps; 10000 continuations of one task, made before
// and after it completes, each run once; a chain of 2000 tasks each waiting for the next (run inline, deep).
PROCEDURE Main()
   ? "fan:", Fan( 20000 )
   ? "tree:", Tree( 12 ):Result
   ? "delays:", Delays( 5000 )
   ? "cancellations:", Cancellations( 5000 )
   ? "races:", Races( 1000 )
   ? "failures:", Failures( 2000 )
   ? "continuations:", Continuations( 10000 )
   ? "chain:", TaskRun( {|| Chain( 2000 ) } ):Result
RETURN

STATIC FUNCTION Fan( n )
   LOCAL a := {}, k, r, nBad := 0
   FOR k := 1 TO n
      AAdd( a, TaskRun( {| i | i * 2 }, k ) )
   NEXT
   r := TaskWhenAll( a ):Result
   FOR k := 1 TO n
      IF r[ k ] != k * 2
         nBad++
      ENDIF
   NEXT
RETURN LTrim( Str( Len( r ) ) ) + " results, " + LTrim( Str( nBad ) ) + " wrong"

// A task whose value is the number of leaves under it.
STATIC FUNCTION Tree( nDepth )
   IF nDepth == 0
      RETURN TaskRun( {|| 1 } )
   ENDIF
RETURN TaskWhenAll( { Tree( nDepth - 1 ), Tree( nDepth - 1 ) } ):ContinueWith( {| t | t:Result[ 1 ] + t:Result[ 2 ] } )

STATIC FUNCTION Delays( n )
   LOCAL a := {}, k, oRan := AtomicInt(), oEarly := AtomicInt(), nStart := ElapsedMs()
   FOR k := 1 TO n
      AAdd( a, Delayed( k % 50 + 1, nStart, oRan, oEarly ) )
   NEXT
   TaskWhenAll( a ):Wait()
RETURN LTrim( Str( oRan:Value ) ) + " ran, " + LTrim( Str( oEarly:Value ) ) + " early"

// A delay of nMs whose continuation counts itself, and whether it ran before nMs after nStart (its own nMs: a
// parameter of this call, not a loop's variable the blocks would share).
STATIC FUNCTION Delayed( nMs, nStart, oRan, oEarly )
RETURN TaskDelay( nMs ):ContinueWith( {|| oRan:Add( 1 ), iif( ElapsedMs() - nStart < nMs, oEarly:Add( 1 ), NIL ) } )

STATIC FUNCTION Cancellations( n )
   LOCAL aS := {}, aD := {}, aR := {}, k, oRan := AtomicInt(), nCanceled := 0
   FOR k := 1 TO n
      AAdd( aS, CancellationTokenSource() )
      AAdd( aD, TaskDelay( 3600000, aS[ k ]:Token ) )
      AAdd( aR, aS[ k ]:Token:Register( {|| oRan:Add( 1 ) } ) )
   NEXT
   FOR k := 1 TO n
      IF k % 2 == 0
         aS[ k ]:Cancel()
      ELSE
         aS[ k ]:CancelAfter( k % 20 )
      ENDIF
   NEXT
   TaskWhenAll( aR ):Wait()
   AEval( aD, {| t | t:Wait(), nCanceled += iif( t:IsCanceled, 1, 0 ) } )
RETURN LTrim( Str( nCanceled ) ) + " delays canceled, " + LTrim( Str( oRan:Value ) ) + " callbacks"

STATIC FUNCTION Races( n )
   LOCAL k, nDelay := 0, t, d
   FOR k := 1 TO n
      d := TaskDelay( 1 )
      t := TaskWhenAny( { TaskCompletionSource():Task, d } )
      IF t:Result == d
         nDelay++
      ENDIF
   NEXT
RETURN nDelay

STATIC FUNCTION Failures( n )
   LOCAL a := {}, k, nErrors := 0, nBreaks := 0, t
   FOR k := 1 TO n
      AAdd( a, iif( k % 2 == 0, TaskRun( {| i | i + "x" }, k ), TaskRun( {| i | Break( i ) }, k ) ) )
   NEXT
   t := TaskWhenAll( a )
   t:Wait()
   AEval( a, {| x | x:Wait(), iif( ValType( x:Error ) == "O", nErrors++, ;
                                   iif( ValType( x:Error ) == "N", nBreaks++, NIL ) ) } )
RETURN LTrim( Str( nErrors ) ) + " errors, " + LTrim( Str( nBreaks ) ) + " breaks, first " + LTrim( Str( t:Error ) )

STATIC FUNCTION Continuations( n )
   LOCAL s := TaskCompletionSource(), a := {}, k, oRan := AtomicInt()
   FOR k := 1 TO n / 2
      AAdd( a, s:Task:ContinueWith( {|| oRan:Add( 1 ) } ) )
   NEXT
   s:SetResult( 1 )
   FOR k := 1 TO n / 2
      AAdd( a, s:Task:ContinueWith( {|| oRan:Add( 1 ) } ) )
   NEXT
   TaskWhenAll( a ):Wait()
RETURN oRan:Value

STATIC FUNCTION Chain( n )
   IF n == 0
      RETURN 0
   ENDIF
RETURN TaskRun( {|| Chain( n - 1 ) } ):Result + 1
