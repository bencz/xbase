// A channel's enumerator awaited: MoveNextAsync() gives a task of whether a value came, taken into Current. Values
// there already, values sent later by another task, the channel closed while a MoveNextAsync() waits (.F.) and closed
// and empty before (.F. at once, again and again), a token that cancels the wait, receivers of both kinds waiting at
// once (ReceiveAsync() and MoveNextAsync() of two enumerators: served in the order they came, each value in its
// enumerator's Current), two enumerators of one channel sharing its values, and the enumerator's MoveNext() beside it.
PROCEDURE Main()
   LOCAL ch := Channel(), e, e2, t, s, a, c, k

   ch:TrySend( 1 )
   ch:TrySend( 2 )
   t := Drain( ch )
   ch:TrySend( 3 )
   Producer( ch, 4, 6 )
   ? "drained:", t:Wait( 30000 ), t:Result

   ch := Channel()
   ch:Close()
   e := ch:GetEnumerator()
   ? "closed and empty:", e:MoveNextAsync():Result, e:MoveNextAsync():Result, e:Current

   ch := Channel()
   e := ch:GetEnumerator()
   t := e:MoveNextAsync()
   ? "waits:", t:IsCompleted
   ch:Close()
   ? "closed while it waits:", t:Wait( 30000 ), t:Result, t:IsFailed

   ch := Channel()
   e := ch:GetEnumerator()
   s := CancellationTokenSource()
   t := e:MoveNextAsync( s:Token )
   s:Cancel()
   ? "canceled:", t:Wait( 30000 ) .OR. .T., t:IsCanceled
   ch:TrySend( "kept" )
   ? "the value went to no canceled waiter:", e:MoveNextAsync():Result, e:Current

   ch := Channel()
   e := ch:GetEnumerator()
   e2 := ch:GetEnumerator()
   a := { ch:ReceiveAsync(), e:MoveNextAsync(), ch:ReceiveAsync(), e2:MoveNextAsync() }
   FOR k := 1 TO 4
      ch:TrySend( k * 10 )
   NEXT
   TaskWhenAll( a ):Wait( 30000 )
   ? "in the order they came:", a[ 1 ]:Result, a[ 2 ]:Result, e:Current, a[ 3 ]:Result, a[ 4 ]:Result, e2:Current

   ch := Channel()
   c := { ch:GetEnumerator(), ch:GetEnumerator() }
   t := { c[ 1 ]:MoveNextAsync(), c[ 2 ]:MoveNextAsync() }
   ch:TrySend( "x" )
   ch:TrySend( "y" )
   TaskWhenAll( t ):Wait( 30000 )
   ? "two enumerators:", c[ 1 ]:Current, c[ 2 ]:Current
   ch:TrySend( "z" )
   ? "MoveNext beside it:", c[ 1 ]:MoveNext(), c[ 1 ]:Current
RETURN

ASYNC FUNCTION Drain( ch )
   LOCAL e := ch:GetEnumerator(), c := ""
   DO WHILE AWAIT e:MoveNextAsync()
      c += LTrim( Str( e:Current ) )
   ENDDO
RETURN c

ASYNC PROCEDURE Producer( ch, nFrom, nTo )
   LOCAL k
   FOR k := nFrom TO nTo
      AWAIT TaskDelay( 5 )
      ch:TrySend( k )
   NEXT
   ch:Close()
RETURN
