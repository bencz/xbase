// AWAIT FOR EACH in ASYNC functions: a channel's values as they come (each MoveNextAsync() awaited, the loop ends when
// the channel is closed and empty); an array of tasks, each awaited in order (an element that is no task is itself);
// a hash's values, a string's characters, a generator's elements; k => v (k a hash's key, else the position, as in
// FOR EACH); the loop variables put back
// after the loop, after EXIT, and when an error leaves it; LOOP; an asynchronous enumerator of a class (MoveNextAsync
// and Current) and an object whose GetAsyncEnumerator() gives one; a value that is no collection (the error FOR EACH
// raises); in the procedure that starts the program, which waits for each step.
PROCEDURE Main()
   LOCAL ch := Channel(), t, x, c := ""
   t := FromChannel( ch )
   Feed( ch )
   ? "channel:", t:Wait( 30000 ), t:Result
   ? "tasks:", Tasks():Wait( 30000 ), Tasks():Result
   ? "kinds:", Kinds():Result
   ? "kept:", Kept():Result
   ? "classes:", Classes():Result
   ? "no collection:", NoCollection():Result
   AWAIT FOR EACH x IN { Later( "s" ), "t", Later( "u" ) }
      c += x
   NEXT
   ? "start procedure:", c
RETURN

ASYNC FUNCTION FromChannel( ch )
   LOCAL x, n := 0, c := ""
   AWAIT FOR EACH x IN ch
      n += x
      c += LTrim( Str( x ) )
   NEXT
RETURN c + " = " + LTrim( Str( n ) )

ASYNC PROCEDURE Feed( ch )
   LOCAL k
   FOR k := 1 TO 5
      AWAIT TaskDelay( 3 )
      ch:TrySend( k )
   NEXT
   ch:Close()
RETURN

ASYNC FUNCTION Tasks()
   LOCAL r, c := ""
   AWAIT FOR EACH r IN { TaskRun( {|| "a" } ), Later( "b" ), "c", TaskFromResult( "d" ), Later( "e" ) }
      c += r
   NEXT
RETURN c

ASYNC FUNCTION Later( x )
   AWAIT TaskDelay( 2 )
RETURN x

ASYNC FUNCTION Kinds()
   LOCAL v, k, c := ""
   AWAIT FOR EACH k => v IN { "one" => 1, "two" => 2 }
      c += k + ":" + LTrim( Str( v ) ) + " "
   NEXT
   AWAIT FOR EACH k => v IN "xyz"
      c += LTrim( Str( k ) ) + v
   NEXT
   AWAIT FOR EACH v IN Numbers( 3 )
      c += LTrim( Str( v ) )
   NEXT
RETURN c

FUNCTION Numbers( n )
   LOCAL k
   FOR k := 1 TO n
      YIELD k * 10
   NEXT
RETURN

ASYNC FUNCTION Kept()
   LOCAL x := "before", k := "key", c := "", e
   AWAIT FOR EACH k => x IN { 1, 2, 3, 4, 5 }
      IF x == 2
         LOOP
      ENDIF
      IF x == 4
         EXIT
      ENDIF
      c += LTrim( Str( x ) )
   NEXT
   c += " " + x + " " + k
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AWAIT FOR EACH x IN { 1, 2 }
         c += " in " + LTrim( Str( x ) )
         ? 1 + "a"
      NEXT
   RECOVER USING e
      c += " error " + e:operation + " " + x
   END SEQUENCE
RETURN c

ASYNC FUNCTION Classes()
   LOCAL x, c := ""
   AWAIT FOR EACH x IN Countdown():New( 3 )
      c += LTrim( Str( x ) )
   NEXT
   AWAIT FOR EACH x IN Source():New()
      c += x
   NEXT
RETURN c

ASYNC FUNCTION NoCollection()
   LOCAL x, e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AWAIT FOR EACH x IN 42
      NEXT
   RECOVER USING e
      RETURN LTrim( Str( e:subCode ) ) + " " + e:description
   END SEQUENCE
RETURN "none"

CLASS Countdown
   VAR n
   VAR Current
   METHOD New( n ) INLINE ::n := n, Self
   ASYNC METHOD MoveNextAsync()
ENDCLASS

ASYNC METHOD MoveNextAsync() CLASS Countdown
   AWAIT TaskDelay( 1 )
   IF ::n == 0
      RETURN .F.
   ENDIF
   ::Current := ::n--
RETURN .T.

CLASS Source
   METHOD New() INLINE Self
   METHOD GetAsyncEnumerator()
ENDCLASS

// A channel's enumerator: two values, then the channel closed.
METHOD GetAsyncEnumerator() CLASS Source
   LOCAL ch := Channel()
   ch:TrySend( "p" )
   ch:TrySend( "q" )
   ch:Close()
RETURN ch:GetEnumerator()
