// A channel's enumerator made with a token (GetEnumerator( oToken )): its MoveNextAsync() calls without a token of
// their own wait under it, so an AWAIT FOR EACH over it ends when the token is canceled (the loop's AWAIT fails as
// canceled, which the program handles), and the value sent afterwards goes to the next receiver, not to the loop that
// was left; a token canceled before the loop ends it at its first step; a MoveNextAsync( oToken ) of its own overrides
// it. One enumerator's MoveNextAsync() asked while another of its own waits raises BASE/9035 (both would take their
// values into one Current), and once the first ended the enumerator waits again; GetEnumerator() of something that
// is no token is the argument error.
#include "error.ch"

PROCEDURE Main()
   LOCAL ch := Channel(), s := CancellationTokenSource(), t, e, s2, x

   t := Consume( ch:GetEnumerator( s:Token ) )
   ch:TrySend( 1 )
   ch:TrySend( 2 )
   AWAIT TaskDelay( 20 )
   s:Cancel()
   ? "canceled loop:", t:Wait( 30000 ), t:Result
   ch:TrySend( "after" )
   ? "the value went on:", ch:ReceiveAsync():Result

   s := CancellationTokenSource()
   s:Cancel()
   ? "canceled before:", Consume( ch:GetEnumerator( s:Token ) ):Result

   s := CancellationTokenSource()
   s2 := CancellationTokenSource()
   e := ch:GetEnumerator( s:Token )
   t := e:MoveNextAsync( s2:Token )
   s:Cancel()
   ? "own token: waits on", t:IsCompleted
   s2:Cancel()
   ? "own token canceled:", t:Wait( 30000 ) .OR. .T., t:IsCanceled

   e := ch:GetEnumerator()
   t := e:MoveNextAsync()
   ? "second while one waits:", Try( {|| e:MoveNextAsync() } )
   ch:TrySend( "one" )
   ? "the first took it:", t:Result, e:Current
   ch:TrySend( "two" )
   ? "then again:", e:MoveNextAsync():Result, e:Current

   ? "no token:", Try( {|| ch:GetEnumerator( "x" ) } ), ValType( ch:GetEnumerator( NIL ) )
RETURN

ASYNC FUNCTION Consume( e )
   LOCAL x, c := "", err
   TRY
      AWAIT FOR EACH x IN e
         c += LTrim( Str( x ) )
      NEXT
      c += " closed"
   CATCH err
      c += " left: " + err:description
   END
RETURN c

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := ValType( Eval( b ) )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
