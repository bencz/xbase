// Frames of resumable functions handed between threads: a frame is saved at each suspension and loaded by the next
// step, which may run on another worker (one step at a time, published by the handoff that schedules it, no lock).
// - 64 ASYNC functions run at once on the pool, 2000 steps each with AWAIT TaskYield() between (each step may resume
//   on another worker): their locals (an array grown each step, a hash counted, a string appended in place, a number,
//   an array made after the first suspension, a codeblock over a local) hold, at the end, exactly what every step
//   left there;
// - an ASYNC function whose caller goes on without awaiting it, and changes the array it passed: the function sees
//   the caller's changes, the caller the function's;
// - a generator stepped by tasks one after another (each MoveNext from another task, so from any worker): its locals
//   carry over.
#define FUNCS 64
#define STEPS 2000

PROCEDURE Main()
   LOCAL a := {}, k, nBad := 0
   FOR k := 1 TO FUNCS
      AAdd( a, Steps( k ) )
   NEXT
   FOR k := 1 TO FUNCS
      IF !( a[ k ]:Result == "ok" )
         ++nBad
         ? k, a[ k ]:Result
      ENDIF
   NEXT
   ? "functions:", IIf( nBad == 0, "every frame whole", Str( nBad ) + " wrong" )
   ? "not awaited:", Shared()
   ? "generator:", Generator()
RETURN

STATIC ASYNC FUNCTION Steps( nId )
   LOCAL k, aGrown := {}, hCount := { "steps" => 0 }, cText := "", nSum := 0, aLate, bAdd, nCaptured := 0
   AWAIT TaskYield()
   aLate := Array( 3 ) // made after the first suspension
   bAdd := {| n | nCaptured += n }
   FOR k := 1 TO STEPS
      AAdd( aGrown, k * nId )
      hCount[ "steps" ]++
      cText += Chr( 65 + k % 26 )
      nSum += k
      aLate[ k % 3 + 1 ] := k
      Eval( bAdd, 1 )
      AWAIT TaskYield()
   NEXT
   FOR k := 1 TO STEPS
      IF aGrown[ k ] != k * nId .OR. !( SubStr( cText, k, 1 ) == Chr( 65 + k % 26 ) )
         RETURN "step " + LTrim( Str( k ) ) + " lost"
      ENDIF
   NEXT
   IF Len( aGrown ) != STEPS .OR. hCount[ "steps" ] != STEPS .OR. Len( cText ) != STEPS .OR. ;
      nSum != STEPS * ( STEPS + 1 ) / 2 .OR. aLate[ 1 ] != STEPS - 2 .OR. nCaptured != STEPS
      RETURN "counts wrong"
   ENDIF
RETURN "ok"

// The caller and the function change one array by turns, through two channels.
STATIC FUNCTION Shared()
   LOCAL aBoth := { 0, 0 }, chGo := Channel( 1 ), chBack := Channel( 1 ), oTask, k
   oTask := Turns( aBoth, chGo, chBack ) // not awaited: the caller goes on
   FOR k := 1 TO 500
      aBoth[ 1 ] := k
      chGo:SendAsync( k ):Wait( 60000 )
      chBack:ReceiveAsync():Wait( 60000 )
      IF aBoth[ 2 ] != k
         RETURN "caller saw" + Str( aBoth[ 2 ] ) + " at" + Str( k )
      ENDIF
   NEXT
   chGo:Close()
RETURN IIf( oTask:Result == 500, "each saw the other's changes", "function saw wrong at" + Str( oTask:Result ) )

STATIC ASYNC FUNCTION Turns( aBoth, chGo, chBack )
   LOCAL n, nSeen := 0
   DO WHILE .T.
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         n := AWAIT chGo:ReceiveAsync()
      RECOVER
         EXIT
      END SEQUENCE
      IF aBoth[ 1 ] != n
         RETURN nSeen
      ENDIF
      aBoth[ 2 ] := n
      ++nSeen
      AWAIT chBack:SendAsync( n )
   ENDDO
RETURN nSeen

// A generator whose MoveNext each of 200 tasks calls in turn: its locals carry over from step to step.
STATIC FUNCTION Generator()
   LOCAL oGen := Counting(), k, nGot := 0, oTask
   FOR k := 1 TO 200
      oTask := TaskRun( {|| IIf( oGen:MoveNext(), oGen:Current, -1 ) } )
      IF oTask:Wait( 60000 ) .AND. oTask:Result == k * ( k + 1 ) / 2
         ++nGot
      ENDIF
   NEXT
RETURN IIf( nGot == 200, "each step from another task", Str( nGot ) )

STATIC FUNCTION Counting()
   LOCAL k, nSum := 0, aKept := {}
   FOR k := 1 TO 200
      nSum += k
      AAdd( aKept, nSum )
      YIELD aKept[ k ]
   NEXT
RETURN
