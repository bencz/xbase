// AWAIT ch:ReceiveAsync(): when a value is there, the AWAIT takes it as its result with no task made (the method
// answers the AWAIT that fused its send); otherwise it waits as before. Checked, in an ASYNC function and outside
// one: values there, a value sent later, values left in a closed channel then its error, a token; and what must stay
// a task: ReceiveAsync() not awaited (kept, awaited later, or tested with IsCompleted) even with a value there, inside
// a compiled method that an AWAIT fused, inside a generator step that AWAIT g:MoveNext() runs, and in a codeblock.
#include "error.ch"

PROCEDURE Main()
   LOCAL ch := Channel( 10 ), x, k, s := 0, o
   FOR k := 1 TO 5
      ch:TrySend( k )
   NEXT
   FOR k := 1 TO 5 // outside an ASYNC function
      s += AWAIT ch:ReceiveAsync()
   NEXT
   ? "outside:", s
   ? "inside:", Inside():Result
   ? "later:", Later():Result
   ? "closed:", Closed():Result
   ? "token:", WithToken():Result
   ch:TrySend( 7 )
   x := ch:ReceiveAsync() // not awaited: a task, completed
   ? "not awaited:", ValType( x ), x:IsCompleted, x:Result
   ch:TrySend( 8 )
   ? "tested:", ch:ReceiveAsync():IsCompleted
   o := Holder():New( ch )
   ch:TrySend( 9 )
   ? "in a fused method:", AWAIT o:Take()
   ? "in a generator step:", GenStep( ch ):Result
   ch:TrySend( 10 )
   ? "in a block:", ValType( Eval( {|| ch:ReceiveAsync() } ) )
RETURN

STATIC ASYNC FUNCTION Inside()
   LOCAL ch := Channel( 100 ), k, s := 0, t
   FOR k := 1 TO 100
      ch:TrySend( k )
   NEXT
   FOR k := 1 TO 99
      s += AWAIT ch:ReceiveAsync()
   NEXT
   t := ch:ReceiveAsync() // kept: a task, awaited after
   s += AWAIT t
RETURN Str( s, 6 ) + " " + ValType( t )

// A receive that waits: the value comes from a task after the receive began.
STATIC ASYNC FUNCTION Later()
   LOCAL ch := Channel( 1 ), x
   TaskRun( {|| Waits( 20 ), ch:TrySend( "late" ) } )
   x := AWAIT ch:ReceiveAsync()
RETURN x

STATIC PROCEDURE Waits( nMs )
   TaskDelay( nMs ):Wait()
RETURN

// Values left in a closed channel are received; then the error "Channel closed".
STATIC ASYNC FUNCTION Closed()
   LOCAL ch := Channel( 3 ), a := {}, oErr
   ch:TrySend( 1 )
   ch:TrySend( 2 )
   ch:Close()
   AAdd( a, AWAIT ch:ReceiveAsync() )
   AAdd( a, AWAIT ch:ReceiveAsync() )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AAdd( a, AWAIT ch:ReceiveAsync() )
   RECOVER USING oErr
      AAdd( a, oErr:description )
   END SEQUENCE
RETURN a[ 1 ] + a[ 2 ] == 3 .AND. a[ 3 ] == "Channel closed" .AND. Len( a ) == 3

// With a token not canceled, and one canceled while the receive waits.
STATIC ASYNC FUNCTION WithToken()
   LOCAL ch := Channel( 2 ), oSource := CancellationTokenSource(), x, lCanceled := .F.
   ch:TrySend( "v" )
   x := AWAIT ch:ReceiveAsync( oSource:Token )
   oSource:CancelAfter( 20 )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AWAIT ch:ReceiveAsync( oSource:Token )
   RECOVER
      lCanceled := .T.
   END SEQUENCE
RETURN x + IIf( lCanceled, " canceled", " not canceled" )

CREATE CLASS Holder
   VAR ch
   METHOD New( ch ) INLINE ::ch := ch, Self
   METHOD Take()
ENDCLASS

// AWAIT o:Take() fuses its send to this method: the receive in it is no AWAIT's, so it is a task.
METHOD Take() CLASS Holder
   LOCAL t := ::ch:ReceiveAsync()
RETURN ValType( t ) + " " + LTrim( Str( t:Result ) )

// AWAIT g:MoveNext() runs the generator's step: the receive in it is a task.
STATIC ASYNC FUNCTION GenStep( ch )
   LOCAL g := Receives( ch ), lMoved
   ch:TrySend( 11 )
   lMoved := AWAIT g:MoveNext()
RETURN IIf( lMoved, g:Current, "no step" )

STATIC FUNCTION Receives( ch )
   LOCAL t := ch:ReceiveAsync()
   YIELD ValType( t ) + " " + LTrim( Str( t:Result ) )
RETURN
