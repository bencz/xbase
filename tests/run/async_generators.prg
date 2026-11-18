// Asynchronous generators (an ASYNC function with YIELD): calling one runs nothing until the first MoveNextAsync();
// AWAIT FOR EACH, MoveNextAsync() by hand (.F. at the end, and again and again), FOR EACH (MoveNext() waits) and a
// consumer that is itself ASYNC; a body that never suspends (each step completed at once), one that yields nothing,
// one that fails after an element (the step fails with the error, the enumeration is over), a second MoveNextAsync()
// while one waits (BASE/9035), a token that cancels a wait (canceled, the enumeration over, and the element the body
// yields after it dropped: Current untouched); generators over
// generators (YIELD inside AWAIT FOR EACH, with EXIT, RETURN and a PRIVATE loop variable put back once the loop ends),
// YIELD inside TRY ... CATCH that recovers, a PRIVATE of the body unseen by the consumer, a consumer on another thread,
// NIL and arrays as elements, an ASYNC METHOD that yields (Self reached between steps), a generator whose first step
// yields on one thread (its body parked there) and whose rest another thread steps, YIELD inside AWAIT FOR EACH inside
// BEGIN SEQUENCE WITH a handler that recovers; 5000 generators left suspended at a YIELD (with an array and a PRIVATE
// each) are collected whole.
MEMVAR nStarted, p

PROCEDURE Main()
   LOCAL x, g, c, t, s, e
   PRIVATE nStarted := 0, p := "outer"

   g := Numbers( 3, 1 )
   ? "lazy:", nStarted
   c := ""
   AWAIT FOR EACH x IN g
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "await each:", c, "started:", nStarted

   g := Numbers( 2, 1 )
   ? "by hand:", AWAIT g:MoveNextAsync(), g:Current, AWAIT g:MoveNextAsync(), g:Current, AWAIT g:MoveNextAsync(), ;
     AWAIT g:MoveNextAsync(), g:Current
   c := ""
   FOR EACH x IN Numbers( 4, 1 )
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "for each:", c, "consumer:", Sum( Numbers( 5, 1 ) ):Result

   g := Numbers( 2, 0 )
   t := g:MoveNextAsync()
   ? "never suspends:", t:IsCompleted, t:Result, g:Current, ( t := g:MoveNextAsync() ):IsCompleted, t:Result
   ? "nothing:", ( AWAIT Numbers( 0, 1 ):MoveNextAsync() )

   g := Fails()
   ? "fails: first", AWAIT g:MoveNextAsync(), g:Current, "then", Try( {|| g:MoveNextAsync():Result } ), ;
     "then", AWAIT g:MoveNextAsync()

   g := Numbers( 3, 200 )
   t := g:MoveNextAsync()
   ? "second while one waits:", Try( {|| g:MoveNextAsync() } ), "first:", t:Result, g:Current

   s := CancellationTokenSource()
   g := Numbers( 3, 300 )
   t := g:MoveNextAsync( s:Token )
   s:Cancel()
   ? "canceled:", t:Wait( 30000 ) .OR. .T., t:IsCanceled, "then", AWAIT g:MoveNextAsync()
   AWAIT TaskDelay( 600 )
   ? "the element yielded after the cancel:", ValType( g:Current )

   c := ""
   AWAIT FOR EACH x IN Doubled( Evens( Numbers( 8, 1 ) ) )
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "chained:", c
   c := ""
   AWAIT FOR EACH x IN Upto( Numbers( 9, 1 ), 30 )
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "exit inside:", c, "then return:", Collect( FirstOver( Numbers( 9, 1 ), 45 ) ):Result
   ? "private loop variable:", Collect( Rebinds( Numbers( 2, 1 ) ) ):Result, p

   ? "try inside:", Collect( Recovers() ):Result
   g := Privates()
   ? "private of the body:", AWAIT g:MoveNextAsync(), g:Current, Type( "q" ), p
   c := ""
   ThreadStart( {|| AEval( { 1 }, {|| c := Collect( Numbers( 3, 1 ) ):Result } ) } ):Join()
   ? "another thread:", c
   g := Odd()
   ? "elements:", AWAIT g:MoveNextAsync(), ValType( g:Current ), AWAIT g:MoveNextAsync(), Len( g:Current )
   ? "method:", Collect( Feed():New():Items( 3 ) ):Result
   g := Numbers( 3, 0 )
   c := iif( g:MoveNextAsync():Result, LTrim( Str( g:Current ) ), "?" )
   ThreadStart( {|| AEval( { 1 }, {|| c += Rest( g ) } ) } ):Join()
   ? "first step here, the rest on a thread:", c
   ? "inside a handled sequence:", Collect( Guarded( Numbers( 4, 1 ) ) ):Result
   ? "abandoned:", Abandoned()
RETURN

// The live objects after a full collection, before and after 5000 generators left at a YIELD: the same few.
STATIC FUNCTION Abandoned()
   LOCAL n0, n1, k, g
   __GcCollect()
   n0 := __GcCollect()
   FOR k := 1 TO 5000
      g := Hold( k )
      g:MoveNextAsync():Wait()
      g:MoveNextAsync():Wait()
   NEXT
   g := NIL
   __GcCollect()
   n1 := __GcCollect()
RETURN iif( n1 - n0 < 50, "collected", "kept " + LTrim( Str( n1 - n0 ) ) )

ASYNC FUNCTION Hold( k )
   LOCAL a := Array( 20 )
   PRIVATE pv := k
   AWAIT TaskYield()
   YIELD k
   YIELD a
   YIELD pv
RETURN

ASYNC FUNCTION Numbers( n, nMs )
   LOCAL k
   nStarted++
   FOR k := 1 TO n
      IF nMs > 0
         AWAIT TaskDelay( nMs )
      ENDIF
      YIELD k * 10
   NEXT
RETURN

ASYNC FUNCTION Sum( g )
   LOCAL x, s := 0
   AWAIT FOR EACH x IN g
      s += x
   NEXT
RETURN s

ASYNC FUNCTION Collect( g )
   LOCAL x, c := ""
   AWAIT FOR EACH x IN g
      c += iif( ValType( x ) == "N", LTrim( Str( x ) ), x ) + " "
   NEXT
RETURN c

// The elements left, through FOR EACH (MoveNext() waits).
STATIC FUNCTION Rest( g )
   LOCAL x, c := ""
   FOR EACH x IN g
      c += " " + LTrim( Str( x ) )
   NEXT
RETURN c

// Each element x as x / ( 40 - x ): the division by zero (at 40) recovered as -1 by the inner sequence.
ASYNC FUNCTION Guarded( g )
   LOCAL x, e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      AWAIT FOR EACH x IN g
         BEGIN SEQUENCE WITH {| e | Break( e ) }
            YIELD x / ( 40 - x )
         RECOVER USING e
            YIELD -1
         END SEQUENCE
      NEXT
   RECOVER USING e
      YIELD "outer " + e:description
   END SEQUENCE
RETURN

ASYNC FUNCTION Fails()
   YIELD "one"
   AWAIT TaskYield()
   YIELD 1 + "a"
RETURN

ASYNC FUNCTION Evens( g )
   LOCAL x
   AWAIT FOR EACH x IN g
      IF x % 20 == 0
         YIELD x
      ENDIF
   NEXT
RETURN

ASYNC FUNCTION Doubled( g )
   LOCAL x
   AWAIT FOR EACH x IN g
      AWAIT TaskYield()
      YIELD x * 2
   NEXT
RETURN

ASYNC FUNCTION Upto( g, nMax )
   LOCAL x
   AWAIT FOR EACH x IN g
      IF x > nMax
         EXIT
      ENDIF
      YIELD x
   NEXT
   YIELD -1
RETURN

ASYNC FUNCTION FirstOver( g, nMin )
   LOCAL x
   AWAIT FOR EACH x IN g
      IF x > nMin
         YIELD x
         RETURN
      ENDIF
   NEXT
RETURN

ASYNC FUNCTION Rebinds( g )
   AWAIT FOR EACH p IN g
      YIELD p
   NEXT
   YIELD p
RETURN

ASYNC FUNCTION Recovers()
   LOCAL k, e
   FOR k := 1 TO 3
      TRY
         YIELD iif( k == 2, 1 + "a", k )
      CATCH e
         YIELD "caught " + e:operation
      END
   NEXT
RETURN

ASYNC FUNCTION Privates()
   PRIVATE q := "inner"
   YIELD q
RETURN

ASYNC FUNCTION Odd()
   YIELD NIL
   YIELD { 1, 2, 3 }
RETURN

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
      x := ValType( x )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x

CLASS Feed
   DATA nBase INIT 100
   ASYNC METHOD Items( n )
ENDCLASS

ASYNC METHOD Items( n ) CLASS Feed
   LOCAL k
   FOR k := 1 TO n
      AWAIT TaskYield()
      YIELD ::nBase + k
   NEXT
RETURN
