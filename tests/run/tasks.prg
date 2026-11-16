// Tasks: TaskRun() with a block or a function reference and their arguments, Result, Wait() (with a limit), the
// states and Error; N tasks computing at once (a matrix of counts), tasks that start and wait for tasks; ContinueWith
// chains and continuations of failed tasks; TaskWhenAll (results in order, the first failure, a cancellation, none)
// and TaskWhenAny; TaskCompletionSource (Set..., TrySet..., an error when set twice); failures raised to the waiter (an
// error through its error block, another BREAK value as a BREAK, a cancellation as an error); the environment a task
// starts with (SET values, PRIVATEs visible) and what does not come back; an error in a task's BEGIN SEQUENCE (no
// RECOVER takes it); a chain of 100000 combinators; TaskWhenAll() of an array another task changes; collections while
// tasks allocate; argument errors.
MEMVAR pShared

PROCEDURE Main()
   LOCAL t, n, i, e, s, a, x, bDefault := ErrorBlock()
   PRIVATE pShared := "creator"

   t := TaskRun( {| x, y | x * y }, 6, 7 )
   ? "result:", t:Result, t:ClassName(), t:IsCompleted, t:IsFailed, t:IsCanceled, t:Error, t:Wait()
   ? "symbol:", TaskRun( @Twice(), "ab" ):Result, "nil:", TaskRun( {|| NIL } ):Result

   FOR EACH n IN { 1, 10, 300 }
      ? "count", n, Count( n ), Count( n ) == n * ( n + 1 ) * ( n + 2 ) / 2
   NEXT
   ? "nested:", TaskRun( {|| Nested( 4 ) } ):Result

   x := TaskFromResult( 0 )
   FOR i := 1 TO 200
      x := x:ContinueWith( {| p | p:Result + 1 } )
   NEXT
   ? "chain:", x:Result
   t := TaskRun( {|| 1 + "a" } )
   ? "after failure:", t:ContinueWith( {| p | iif( p:IsFailed, "failed " + p:Error:operation, "ran" ) } ):Result

   a := {}
   FOR i := 1 TO 5
      AAdd( a, TaskRun( {| k | k * 10 }, i ) )
   NEXT
   ? "all:", ArrayText( TaskWhenAll( a ):Result ), Len( TaskWhenAll( {} ):Result )
   s := TaskCompletionSource()
   t := TaskWhenAll( { TaskFromResult( 1 ), TaskRun( {|| Break( "first" ) } ), s:Task } )
   s:SetResult( 3 )
   t:Wait()
   ? "all failed:", t:IsFailed, t:Error
   s := TaskCompletionSource()
   s:SetCanceled()
   ? "all canceled:", TaskWhenAll( { TaskFromResult( 1 ), s:Task } ):IsCanceled
   s := TaskCompletionSource()
   x := TaskFromResult( "ready" )
   ? "any:", TaskWhenAny( { s:Task, x } ):Result == x, TaskWhenAny( { s:Task, x } ):Result:Result

   s := TaskCompletionSource()
   t := TaskRun( {|| s:Task:Result + 1 } )
   ? "source:", s:Task:IsCompleted, s:SetResult( 41 ), t:Result, s:TrySetResult( 1 ), s:TrySetError( "x" ), ;
     s:TrySetCanceled(), s:Task:Result, s:ClassName(), ArrayText( ObjMethods( s ) )
   ? "wait limit:", TaskCompletionSource():Task:Wait( 20 ), TaskCompletionSource():Task:Wait( 0 )
   ? "methods:", ArrayText( ObjMethods( t ) ), Len( ObjVars( t ) )

   ErrorBlock( {| e | Break( e ) } )
   FOR EACH x IN { {|| TaskRun( {|| 1 + "a" } ):Result }, {|| TaskRun( {|| Break( "value" ) } ):Result }, ;
                   {|| Canceled():Result }, {|| SetTwice() }, {|| TaskRun( 5 ) }, {|| TaskWhenAll( "x" ) }, ;
                   {|| TaskWhenAll( { 1 } ) }, {|| TaskWhenAny( {} ) }, {|| TaskFromResult( 1 ):ContinueWith( 5 ) }, ;
                   {|| TaskFromResult( 1 ):Wait( "x" ) } }
      ? "raised:", Caught( x )
   NEXT

   SET DATE FORMAT TO "yyyy.mm.dd"
   SET DECIMALS TO 4
   ? "env:", TaskRun( {|| DToC( SToD( "20261116" ) ) + " " + Str( 1 / 3 ) + " " + pShared } ):Result
   ? "private:", TaskRun( {|| pShared := "task" } ):Result, pShared
   ? "own SET:", TaskRun( {|| Set( _SET_DECIMALS, 1 ) } ):Result, Set( _SET_DECIMALS )
   SET DATE FORMAT TO "mm/dd/yy"
   SET DECIMALS TO 2
   ErrorBlock( bDefault )
   t := TaskRun( {|| Legacy() } )
   t:Wait()
   ? "legacy sequence:", t:IsFailed, t:Error:operation
   ? "deep combinators:", Deep( 100000 )
   ? "changing array:", Changing()
   ? "collections:", Garbage()
RETURN

// Clipper code that uses BEGIN SEQUENCE for BREAKs of its own: an error in it fails the task, as it would end the
// program outside one; its RECOVER does not take it.
STATIC FUNCTION Legacy()
   LOCAL x
   BEGIN SEQUENCE
      x := 1 + "a"
   RECOVER
      RETURN "recovered"
   END SEQUENCE
RETURN "not reached"

// A chain of n TaskWhenAny() over one source, completed by a task: each completion completes the next, in constant
// stack.
STATIC FUNCTION Deep( n )
   LOCAL s := TaskCompletionSource(), t, i
   t := s:Task
   FOR i := 1 TO n
      t := TaskWhenAny( { t } )
   NEXT
   TaskRun( {|| s:SetResult( "end" ) } ):Wait()
   FOR i := 1 TO n
      t := t:Result
   NEXT
RETURN t:Result

// TaskWhenAll() of an array a task keeps changing (its elements, its size): the tasks it found are those it waits for.
STATIC FUNCTION Changing()
   LOCAL a := { TaskFromResult( 1 ), TaskFromResult( 2 ) }, oStop := AtomicInt(), t, i, nGood := 0, x
   t := TaskRun( {|| Flip( a, oStop ) } )
   FOR i := 1 TO 3000
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         x := TaskWhenAll( a ):Result
         IF ValType( x ) == "A"
            nGood++
         ENDIF
      RECOVER
         nGood++ // an element that was no task then: an argument error
      END SEQUENCE
   NEXT
   oStop:Value := 1
   t:Wait()
RETURN nGood


STATIC FUNCTION Flip( a, oStop )
   LOCAL n := 0
   DO WHILE oStop:Value == 0
      IF ++n % 2 == 0
         ASize( a, 1 )
      ELSE
         AAdd( a, iif( n % 3 == 0, "no task", TaskFromResult( n ) ) )
         a[ 1 ] := iif( n % 5 == 0, NIL, TaskFromResult( 0 ) )
      ENDIF
   ENDDO
RETURN n

STATIC FUNCTION Twice( c )
RETURN c + c

// n tasks, each adding up 1 .. k three ways: the sum of their results.
STATIC FUNCTION Count( n )
   LOCAL a := {}, k, nSum := 0
   FOR k := 1 TO n
      AAdd( a, TaskRun( {| m | Sum( m ) + Sum( m ) + Sum( m ) }, k ) )
   NEXT
   AEval( a, {| t | nSum += t:Result } )
RETURN nSum

STATIC FUNCTION Sum( m )
   LOCAL i, s := 0
   FOR i := 1 TO m
      s += i
   NEXT
RETURN s

STATIC FUNCTION Nested( n )
   IF n == 0
      RETURN 1
   ENDIF
RETURN TaskRun( {|| Nested( n - 1 ) } ):Result + TaskRun( {|| Nested( n - 1 ) } ):Result

STATIC FUNCTION Canceled()
   LOCAL s := TaskCompletionSource()
   s:SetCanceled()
RETURN s:Task

STATIC FUNCTION SetTwice()
   LOCAL s := TaskCompletionSource()
   s:SetResult( 1 )
   s:SetError( "again" )
RETURN NIL

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      IF ValType( e ) == "O"
         RETURN e:subSystem + "/" + LTrim( Str( e:genCode ) ) + "/" + LTrim( Str( e:subCode ) ) + " " + ;
                e:operation + " " + e:description
      ENDIF
      RETURN "break " + e
   END SEQUENCE
RETURN "none"

// Tasks that make garbage and collect, each building a list it checks.
STATIC FUNCTION Garbage()
   LOCAL a := {}, k, nGood := 0
   FOR k := 1 TO 40
      AAdd( a, TaskRun( {| nMe | Built( nMe ) }, k ) )
   NEXT
   AEval( a, {| t | nGood += t:Result } )
RETURN nGood

STATIC FUNCTION Built( nMe )
   LOCAL a := {}, i, nGood := 0
   FOR i := 1 TO 3000
      AAdd( a, { nMe, i, Str( i ) } )
   NEXT
   IF nMe % 10 == 0
      __GcCollect( nMe % 20 != 0 )
   ENDIF
   FOR i := 1 TO Len( a )
      IF a[ i, 1 ] == nMe .AND. a[ i, 2 ] == i .AND. Val( a[ i, 3 ] ) == i
         nGood++
      ENDIF
   NEXT
RETURN iif( nGood == 3000, 1, 0 )

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += iif( Empty( c ), "", "," ) + iif( ValType( x ) == "N", LTrim( Str( x ) ), x )
   NEXT
RETURN c
