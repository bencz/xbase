// The SET values an ASYNC function inherits are its caller's as they were at the call, shared until one of the two
// changes them (each change takes a copy first): the caller's change after the call never reaches the function, and
// the function's own change never reaches its caller nor the other functions sharing them. Checked with 1000
// functions suspended at once on the caller's settings while the caller changes SET EXACT, SET DATE FORMAT, SET
// EPOCH, __SetCentury() and SET DELETED; half of them change theirs after resuming, each to its own value. Then where
// a suspended function's environment is kept between steps: functions that suspend and end again and again on a thread
// while collections run (an environment that ends while a thread's recent list holds it is freed by the collector),
// and functions a thread suspended before it ended (its recent list goes to the orphans): each resumes with its values;
// threads that end so while another thread collects all the time (the collector never reads a list the thread let go).
#include "set.ch"
#define FUNCS 1000
MEMVAR cMine

PROCEDURE Main()
   ? "inherited, then the caller's change:", Inherited()
   ? "ended while recent:", EndedRecent()
   ? "suspended by a thread that ended:", Orphaned()
   ? "threads ending while another collects:", EndingThreads()
RETURN

STATIC FUNCTION Inherited()
   LOCAL aGo := {}, aT := {}, k, nBad := 0, cCaller
   SET EXACT ON
   SET DATE FORMAT TO "dd.mm.yyyy"
   SET EPOCH TO 1950
   SET DELETED OFF
   FOR k := 1 TO FUNCS
      AAdd( aGo, TaskCompletionSource() )
      AAdd( aT, Shares( k, aGo[ k ] ) )
   NEXT
   SET EXACT OFF // the caller's own from now on: the functions keep theirs
   SET DATE FORMAT TO "yyyy/mm/dd"
   SET EPOCH TO 2000
   __SetCentury( .F. )
   SET DELETED ON
   AEval( aGo, {| o | o:SetResult( .T. ) } )
   TaskWhenAll( aT ):Wait( 60000 )
   AEval( aT, {| o | IIf( o:Result == "ok", NIL, ( ++nBad, QOut( o:Result ) ) ) } )
   cCaller := IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + Set( _SET_DATEFORMAT ) + " " + ;
      LTrim( Str( Set( _SET_EPOCH ) ) ) + " " + IIf( Set( _SET_DELETED ), "ON", "OFF" )
   SET DATE FORMAT TO "mm/dd/yyyy"
   SET EPOCH TO 1900
   SET DELETED OFF
RETURN IIf( nBad == 0, "each kept its own", LTrim( Str( nBad ) ) + " wrong" ) + "; caller: " + cCaller

// What it sees after its caller changed its own; then, half of them, a change of its own, seen in its next step.
STATIC ASYNC FUNCTION Shares( k, oGo )
   LOCAL cSeen, cOwn
   AWAIT oGo:Task
   cSeen := IIf( Set( _SET_EXACT ), "ON", "OFF" ) + " " + Set( _SET_DATEFORMAT ) + " " + ;
      LTrim( Str( Set( _SET_EPOCH ) ) ) + " " + IIf( Set( _SET_DELETED ), "ON", "OFF" ) + " " + ;
      DToC( SToD( "20240131" ) )
   IF !( cSeen == "ON dd.mm.yyyy 1950 OFF 31.01.2024" )
      RETURN "saw " + cSeen
   ENDIF
   IF k % 2 == 0
      SET DATE FORMAT TO "dd/mm/yy"
      SET EPOCH TO 1900 + Int( k % 100 )
      __SetCentury( .T. )
      AWAIT TaskYield()
      cOwn := Set( _SET_DATEFORMAT ) + " " + LTrim( Str( Set( _SET_EPOCH ) ) )
      IF !( cOwn == "DD/MM/YYYY " + LTrim( Str( 1900 + Int( k % 100 ) ) ) )
         RETURN "own " + cOwn
      ENDIF
   ENDIF
RETURN "ok"

// 20 rounds of 200 functions that suspend once and end, collections between the rounds and inside them.
STATIC FUNCTION EndedRecent()
   LOCAL nRound, aT, k, nSum := 0
   FOR nRound := 1 TO 20
      aT := {}
      FOR k := 1 TO 200
         AAdd( aT, Brief( k ) )
      NEXT
      __GcCollect( .T. )
      TaskWhenAll( aT ):Wait( 60000 )
      AEval( aT, {| o | nSum += o:Result } )
      __GcCollect( nRound % 2 == 0 )
   NEXT
RETURN nSum == 20 * 200 * 201 / 2

STATIC ASYNC FUNCTION Brief( k )
   LOCAL aMine := { k }
   AWAIT TaskYield()
   __GcCollect( .T. )
RETURN aMine[ 1 ]

// A thread starts 100 functions that suspend there, each with a PRIVATE and an ErrorBlock() only its environment
// holds (made just before), then ends; young collections run (only the recent lists, the orphans among them, show
// them those environments); the functions resume elsewhere and find them whole.
STATIC FUNCTION Orphaned()
   LOCAL oGo := TaskCompletionSource(), aT, nSum := 0, k
   aT := ThreadStart( {|| Starts( oGo ) } ):Join()
   FOR k := 1 TO 3
      Garbage( 20000 )
      __GcCollect( .T. )
   NEXT
   oGo:SetResult( .T. )
   TaskWhenAll( aT ):Wait( 60000 )
   AEval( aT, {| o | nSum += o:Result } )
RETURN nSum == 100 * 101 / 2

STATIC FUNCTION Starts( oGo )
   LOCAL a := {}, k
   FOR k := 1 TO 100
      AAdd( a, Waiting( k, oGo ) )
   NEXT
RETURN a

STATIC ASYNC FUNCTION Waiting( k, oGo )
   LOCAL aMine := { k, Replicate( "x", k ) }
   PRIVATE cMine := Replicate( "p", k ) + "!"
   SET EPOCH TO 1800 + k
   ErrorBlock( {|| { k, "block" } } )
   AWAIT oGo:Task
   IF Set( _SET_EPOCH ) != 1800 + k .OR. Len( aMine[ 2 ] ) != k .OR. !( M->cMine == Replicate( "p", k ) + "!" ) .OR. ;
      !( Eval( ErrorBlock() )[ 2 ] == "block" ) .OR. Eval( ErrorBlock() )[ 1 ] != k
      RETURN 0
   ENDIF
RETURN aMine[ 1 ]

// Objects that become garbage: the young collections that follow move what survives.
STATIC PROCEDURE Garbage( n )
   LOCAL k, a
   FOR k := 1 TO n
      a := { k, Str( k ) }
   NEXT
RETURN

// 160 threads, 8 at a time, each starting 20 functions that suspend, then ending; a thread collects (young and full)
// and allocates until they all ended. Then every function resumes.
STATIC FUNCTION EndingThreads()
   LOCAL oGo := TaskCompletionSource(), aT := {}, aStop := { .F. }, oCollector, aThreads, k, j, nSum := 0
   oCollector := ThreadStart( {|| Collects( aStop ) } )
   FOR k := 1 TO 20
      aThreads := {}
      FOR j := 1 TO 8
         AAdd( aThreads, ThreadStart( {|| Starts20( oGo ) } ) )
      NEXT
      AEval( aThreads, {| o | AEval( o:Join(), {| t | AAdd( aT, t ) } ) } )
   NEXT
   aStop[ 1 ] := .T.
   oCollector:Join()
   oGo:SetResult( .T. )
   TaskWhenAll( aT ):Wait( 60000 )
   AEval( aT, {| o | nSum += o:Result } )
RETURN Len( aT ) == 3200 .AND. nSum == 160 * 20 * 21 / 2

STATIC FUNCTION Starts20( oGo )
   LOCAL a := {}, k
   FOR k := 1 TO 20
      AAdd( a, Waiting( k, oGo ) )
   NEXT
RETURN a

STATIC PROCEDURE Collects( aStop )
   LOCAL n := 0
   DO WHILE !aStop[ 1 ]
      Garbage( 500 )
      __GcCollect( ++n % 4 != 0 )
   ENDDO
RETURN
