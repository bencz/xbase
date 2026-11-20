// One cancellation token shared by many tasks on several workers, which wait with it at once (a token's continuations
// go into partitions once it is busy: each thread pushes into one of its own). Checked on such a token, past the
// point where it is partitioned:
// - receives waiting with it on channels that never get a value, made by 32 tasks, all canceled by Cancel(), each
//   once, none left waiting;
// - tasks that keep making such waits while Cancel() runs: every wait made ends canceled (one made after the
//   cancellation at once), none left waiting, in 20 rounds;
// - Register() callbacks made by 16 tasks: each runs once after Cancel(), none before;
// - waits made after the cancellation: canceled at once;
// - sources linked to it after it was partitioned: canceled with it, but one disposed first;
// - CancelAfter(): the waits canceled when its time comes;
// - memory: 16 tasks making 20000 waits each that a value ends, with the one token, and every 100 one that stays
//   waiting (the 99 made before it end behind it in its partition's list, kept until the list is pruned): the heap
//   occupies a few MB more after, with those 3200 still waiting (a partition keeps only what still waits).
#define TASKS 32
#define WAITS 100

PROCEDURE Main()
   Check( "pending canceled:", Pending(), "all 3200" )
   Check( "racing cancel:", Racing(), "every wait canceled" )
   Check( "callbacks:", Callbacks(), "each ran once" )
   Check( "after cancel:", After(), "canceled when made" )
   Check( "linked:", Linked(), "canceled with it, the disposed one not" )
   Check( "cancel after:", CancelLater(), "all canceled" )
   Check( "memory:", Memory(), "bounded" )
RETURN

// Shows a check's outcome; a wrong one ends the program at once (the waits it left would hold the next checks).
STATIC PROCEDURE Check( cWhat, cGot, cExpected )
   ? cWhat, cGot
   IF !( cGot == cExpected )
      ErrorLevel( 1 )
      QUIT
   ENDIF
RETURN

// Waits made by TASKS tasks (WAITS each) on channels of their own that never get a value, with one token: all of
// them canceled by Cancel().
STATIC FUNCTION Pending()
   LOCAL oSource := CancellationTokenSource(), aMakers := {}, aWaits := {}, k, nCanceled := 0, nOther := 0
   FOR k := 1 TO TASKS
      AAdd( aMakers, TaskRun( {|| Waits( oSource:Token, WAITS ) } ) )
   NEXT
   FOR k := 1 TO TASKS
      aMakers[ k ]:Wait( 60000 )
      AEval( aMakers[ k ]:Result, {| o | AAdd( aWaits, o ) } )
   NEXT
   IF AScan( aWaits, {| o | o:IsCompleted } ) != 0
      RETURN "a wait ended before the cancellation"
   ENDIF
   oSource:Cancel()
   TaskWhenAll( aWaits ):Wait( 20000 ) // a wait left would hold it till its limit
   FOR k := 1 TO Len( aWaits )
      IF aWaits[ k ]:IsCanceled
         ++nCanceled
      ELSE
         ++nOther
      ENDIF
   NEXT
RETURN IIf( nCanceled == TASKS * WAITS .AND. nOther == 0, "all " + LTrim( Str( nCanceled ) ), ;
   Str( nCanceled ) + " canceled," + Str( nOther ) + " not" )

// n receives waiting with oToken on a channel of their own that never gets a value.
STATIC FUNCTION Waits( oToken, n )
   LOCAL ch := Channel(), a := Array( n ), k
   FOR k := 1 TO n
      a[ k ] := ch:ReceiveAsync( oToken )
   NEXT
RETURN a

// 16 tasks make waits with one token until they see it canceled, while this thread cancels it (at once in even
// rounds, after 3 ms in odd ones): every wait made ends canceled, in 20 rounds.
STATIC FUNCTION Racing()
   LOCAL nRound, oSource, aMakers, k, aWaits, nLeft := 0, nTotal := 0, nStart
   FOR nRound := 1 TO 20
      oSource := CancellationTokenSource()
      aMakers := {}
      FOR k := 1 TO 16
         AAdd( aMakers, TaskRun( {|| UntilCanceled( oSource:Token ) } ) )
      NEXT
      nStart := ElapsedMs()
      DO WHILE nRound % 2 == 1 .AND. ElapsedMs() - nStart < 3 // odd rounds: after the tasks made some
      ENDDO
      oSource:Cancel()
      FOR k := 1 TO 16
         aMakers[ k ]:Wait( 60000 )
         aWaits := aMakers[ k ]:Result
         nTotal += Len( aWaits )
         TaskWhenAll( aWaits ):Wait( 20000 )
         AEval( aWaits, {| o | IIf( o:IsCanceled, NIL, ++nLeft ) } )
      NEXT
   NEXT
RETURN IIf( nLeft == 0 .AND. nTotal >= 20 * 16, "every wait canceled", Str( nLeft ) + " left of" + Str( nTotal ) )

// Waits made with oToken (at least one, at most 5000) until it is canceled.
STATIC FUNCTION UntilCanceled( oToken )
   LOCAL ch := Channel(), a := {}
   DO WHILE Len( a ) < 5000
      AAdd( a, ch:ReceiveAsync( oToken ) )
      IF oToken:IsCancellationRequested
         EXIT
      ENDIF
   ENDDO
RETURN a

// 16 tasks register 100 callbacks each on one token: none runs before Cancel(), each runs once after it.
STATIC FUNCTION Callbacks()
   LOCAL oSource := CancellationTokenSource(), oRan := AtomicInt(), aMakers := {}, aTasks := {}, k, nEarly
   FOR k := 1 TO 16
      AAdd( aMakers, TaskRun( {|| Registers( oSource:Token, oRan, 100 ) } ) )
   NEXT
   FOR k := 1 TO 16
      aMakers[ k ]:Wait( 60000 )
      AEval( aMakers[ k ]:Result, {| o | AAdd( aTasks, o ) } )
   NEXT
   nEarly := oRan:Value
   oSource:Cancel()
   TaskWhenAll( aTasks ):Wait( 20000 )
RETURN IIf( nEarly == 0 .AND. oRan:Value == 1600 .AND. AScan( aTasks, {| o | !o:IsCompleted } ) == 0, ;
   "each ran once", Str( nEarly ) + Str( oRan:Value ) )

STATIC FUNCTION Registers( oToken, oRan, n )
   LOCAL a := Array( n ), k
   FOR k := 1 TO n
      a[ k ] := oToken:Register( {|| oRan:Add( 1 ) } )
   NEXT
RETURN a

// A token made busy (partitioned), canceled: 200 waits made after it, from 8 tasks, are canceled when made.
STATIC FUNCTION After()
   LOCAL oSource := CancellationTokenSource(), aMakers := {}, k, nNow := 0
   Waits( oSource:Token, 200 )
   oSource:Cancel()
   FOR k := 1 TO 8
      AAdd( aMakers, TaskRun( {|| Waits( oSource:Token, 25 ) } ) )
   NEXT
   FOR k := 1 TO 8
      aMakers[ k ]:Wait( 60000 )
      AEval( aMakers[ k ]:Result, {| o | IIf( o:IsCanceled, ++nNow, NIL ) } )
   NEXT
RETURN IIf( nNow == 200, "canceled when made", Str( nNow ) )

// Sources linked to a busy token (from several tasks): canceled with it; one disposed before is not.
STATIC FUNCTION Linked()
   LOCAL oSource := CancellationTokenSource(), aMakers := {}, aLinked := {}, k, oDisposed, nCanceled := 0
   FOR k := 1 TO 8
      AAdd( aMakers, TaskRun( {|| Waits( oSource:Token, 20 ), CancellationTokenSource( oSource:Token ) } ) )
   NEXT
   AEval( aMakers, {| o | o:Wait( 60000 ), AAdd( aLinked, o:Result ) } )
   oDisposed := CancellationTokenSource( oSource:Token )
   oDisposed:Dispose()
   oSource:Cancel()
   AEval( aLinked, {| o | IIf( o:IsCancellationRequested, ++nCanceled, NIL ) } )
RETURN IIf( nCanceled == 8 .AND. !oDisposed:IsCancellationRequested, "canceled with it, the disposed one not", ;
   Str( nCanceled ) + " " + IIf( oDisposed:IsCancellationRequested, "disposed canceled", "" ) )

// A busy token canceled by CancelAfter( 50 ): its waits end canceled.
STATIC FUNCTION CancelLater()
   LOCAL oSource := CancellationTokenSource(), aMakers := {}, aWaits := {}, k, nCanceled := 0
   FOR k := 1 TO 8
      AAdd( aMakers, TaskRun( {|| Waits( oSource:Token, 30 ) } ) )
   NEXT
   AEval( aMakers, {| o | o:Wait( 60000 ), AEval( o:Result, {| w | AAdd( aWaits, w ) } ) } )
   oSource:CancelAfter( 50 )
   TaskWhenAll( aWaits ):Wait( 20000 )
   AEval( aWaits, {| o | IIf( o:IsCanceled, ++nCanceled, NIL ) } )
RETURN IIf( nCanceled == 240, "all canceled", Str( nCanceled ) )

// 16 tasks: 20000 receives each with one token, each ended by a value, and every 100 a receive that stays waiting:
// the heap occupies a few MB more, those waits and their channels included (without pruning, the 316800 ended ones
// kept behind them took 110 MB).
STATIC FUNCTION Memory()
   LOCAL oSource := CancellationTokenSource(), aTasks := {}, k, nBefore, nAfter, cResult
   __GcCollect()
   nBefore := __GcMemory()[ 4 ]
   FOR k := 1 TO 16
      AAdd( aTasks, TaskRun( {|| Pairs( oSource:Token, 20000 ) } ) )
   NEXT
   TaskWhenAll( aTasks ):Wait( 60000 )
   __GcCollect()
   nAfter := __GcMemory()[ 4 ]
   cResult := IIf( nAfter - nBefore < 32 * 1024 * 1024, "bounded", "grew" + Str( nAfter - nBefore ) )
   AEval( aTasks, {| o | IIf( o:Result[ 1 ] == 0, NIL, cResult := "receives not ended" ) } )
   oSource:Cancel() // the waits left end
RETURN cResult

// n receives with oToken, in batches of 99 on empty channels (each waits, so each is pushed into the token's lists),
// then a receive on a channel that never gets a value, left waiting (pushed after them: they end behind it in its
// list), then the 99 ended by the value TrySend() gives each: { the receives that did not end, the waits left }.
STATIC FUNCTION Pairs( oToken, n )
   LOCAL aCh := Array( 99 ), aGot := Array( 99 ), chNever := Channel(), aLeft := {}, k, j, nNotEnded := 0
   AEval( aCh, {| x, i | aCh[ i ] := Channel() } )
   FOR k := 1 TO n STEP 100
      FOR j := 1 TO 99
         aGot[ j ] := aCh[ j ]:ReceiveAsync( oToken )
      NEXT
      AAdd( aLeft, chNever:ReceiveAsync( oToken ) )
      FOR j := 1 TO 99
         IF !aCh[ j ]:TrySend( j ) .OR. !aGot[ j ]:IsCompleted .OR. aGot[ j ]:Result != j
            ++nNotEnded
         ENDIF
      NEXT
   NEXT
RETURN { nNotEnded, aLeft }
