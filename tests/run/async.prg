// ASYNC functions and AWAIT: the first step runs at the call, on the caller's thread; an AWAIT of a task that has not
// ended suspends, and the function goes on after it on a worker once the task ended, with its LOCALs, parameters,
// loop state, sequences and WITH OBJECT as they were. Checked: AWAIT of values that are no task and of tasks that ended
// (no suspension), AWAIT anywhere in an expression (arguments, short-circuits, IIF, compound and indexed assignments,
// literals, unary minus, conditions of loops, SWITCH), loops left and continued after AWAIT, codeblocks that share a
// LOCAL across AWAIT, recursion, methods, procedures, errors and BREAK values across AWAIT (TRY, RECOVER, handlers,
// ALWAYS), cancellation, the environment (SET values and work areas of the function's own, PRIVATEs of the caller),
// the call stack, and the Task API on the tasks returned.
PROCEDURE Main()
   LOCAL t, o, a
   ? "-- values"
   t := Add( 2, 3 )
   ? "sync:", ValType( t ), t:IsCompleted, t:Result, AWAIT t
   ? "no task:", AWAIT 7, AWAIT "s", AWAIT NIL, ValType( AWAIT { 1 } )
   ? "ended task:", AWAIT TaskFromResult( "done" )
   t := Later( 5 )
   ? "suspended:", t:IsCompleted
   ? "later:", AWAIT t, t:IsCompleted
   ? "chain:", AWAIT Chain( 200 )
   ? "loop:", AWAIT Loop( 300 )
   ? "-- expressions"
   ? AWAIT Exprs()
   ? "block:", AWAIT BlockShares()
   ? "each:", AWAIT EachAcross( { 1, 2, 3 }, { "a" => 1, "b" => 2 }, "xyz" )
   ? "-- errors"
   t := Fails()
   t:Wait()
   ? "failed:", t:IsFailed, t:Error:Description, t:Error:SubCode
   ? "first step fails:", FailsAtOnce():IsFailed
   ? "caught:", AWAIT Catcher()
   ? "seq:", AWAIT SeqAcross()
   ? "always:", AWAIT AlwaysAcross()
   ? "break value:", AWAIT BreakCatch()
   ? "handler:", AWAIT HandlerAcross()
   ? "error block set inside:", AWAIT OwnErrorBlock()
   a := AWAIT ReturnThroughAlways()
   ? "return through finally:", Len( a ), a[ 1 ], a[ 2 ]
   ? "canceled:", AWAIT Canceled()
   ? "-- objects"
   o := Acc():New()
   ? "method:", AWAIT o:AddLater( 5 ), AWAIT o:AddLater( 6 ), o:n
   ? "with object:", AWAIT WithAcross( o )
   ? "procedure:", ValType( AWAIT Proc() )
   ? "static:", AWAIT Hidden( "x" )
   ? "-- environment"
   ? AWAIT EnvOwn()
   ? AWAIT Privates()
   ? AWAIT OwnPrivates(), "after:", Type( "cMine" ) + Type( "zz" )
   ? AWAIT DateFormats(), "caller:", Set( _SET_DATEFORMAT )
   ? "tables:", AWAIT Tables(), Used(), Select()
   ? "call stack:", AWAIT Names()
   ? "-- tasks"
   ? "when all:", ArrStr( AWAIT TaskWhenAll( { Later( 1 ), Add( 1, 1 ), Later( 2 ) } ) )
   ? "when any:", ( AWAIT TaskWhenAny( { TaskCompletionSource():Task, Later( 3 ) } ) ):Result
   ? "continue with:", Later( 4 ):ContinueWith( {| x | x:Result * 2 } ):Result
   ? "result:", Later( 6 ):Result, "wait:", Later( 7 ):Wait()
RETURN

ASYNC FUNCTION Add( a, b )
RETURN a + b

ASYNC FUNCTION Later( n )
   LOCAL v := AWAIT TaskDelay( 2 ), w
   w := AWAIT TaskRun( {|| n * 100 } )
RETURN w + n + iif( v == NIL, 0, 1000 )

ASYNC FUNCTION Chain( n )
   IF n == 0
      AWAIT TaskDelay( 1 )
      RETURN 0
   ENDIF
RETURN 1 + AWAIT Chain( n - 1 )

ASYNC FUNCTION Loop( n )
   LOCAL k, s := 0
   FOR k := 1 TO n
      s += AWAIT iif( k % 3 == 0, TaskRun( {| i | i }, k ), k )
   NEXT
RETURN s

STATIC FUNCTION D( x )
RETURN TaskDelay( 1 ):ContinueWith( {|| x } )

STATIC FUNCTION Sum3( a, b, c )
RETURN a * 100 + b * 10 + c

ASYNC FUNCTION Exprs()
   LOCAL a := { 10, 20, 30 }, h := { => }, s := "", n := 5, k, c := "ab", lT := .T., x
   s += Str( Sum3( 1, AWAIT D( 2 ), 3 ), 3 )
   s += iif( lT .AND. ( AWAIT D( .T. ) ), " and", " noand" )
   s += iif( !lT .OR. ( AWAIT D( .F. ) ), " or", " noor" )
   s += " " + iif( n > 3, AWAIT D( "big" ), AWAIT D( "small" ) )
   n += AWAIT D( 10 )
   s += Str( n, 3 )
   a[ AWAIT D( 2 ) ] := AWAIT D( 99 )
   s += Str( a[ 2 ], 3 )
   h[ AWAIT D( "k" ) ] := { AWAIT D( 1 ), AWAIT D( 2 ) }
   s += Str( h[ "k" ][ 2 ], 2 )
   s += " " + c + AWAIT D( "cd" ) + c
   s += Str( - AWAIT D( 4 ), 3 )
   FOR k := 10 TO 1 STEP -3
      x := AWAIT D( k )
      IF x == 4
         EXIT
      ENDIF
      s += Str( x, 3 )
   NEXT
   s += Str( k, 3 )
   n := 7
   DO WHILE ( x := AWAIT D( n-- ) ) > 4
      IF x == 6
         LOOP
      ENDIF
      s += Str( x, 2 )
   ENDDO
   SWITCH AWAIT D( 2 )
   CASE 1
      s += " one"
      EXIT
   CASE 2
      s += " two"
      EXIT
   ENDSWITCH
   s += Str( Len( { AWAIT D( 1 ), AWAIT D( 2 ), 3 } ), 2 )
   s += " " + Upper( AWAIT D( "lo" ) ) + Str( Len( AWAIT D( { 1, 2 } ) ), 2 )
RETURN s

// A codeblock made before AWAIT and the function share the LOCAL after it.
ASYNC FUNCTION BlockShares()
   LOCAL n := 1, b := {|| n * 10 }
   AWAIT D( 0 )
   n := 4
   AWAIT D( 0 )
   Eval( {|| n++ } )
RETURN Eval( b )

ASYNC FUNCTION EachAcross( aIn, hIn, cIn )
   LOCAL x, k, s := ""
   FOR EACH k => x IN aIn
      s += Str( AWAIT D( x * 10 ), 3 ) + "@" + LTrim( Str( k ) )
   NEXT
   FOR EACH k => x IN hIn
      s += " " + k + "=" + LTrim( Str( AWAIT D( x ) ) )
   NEXT
   FOR EACH x IN cIn DESCEND
      s += " " + AWAIT D( x )
   NEXT
RETURN s

ASYNC FUNCTION Fails()
   AWAIT TaskDelay( 1 )
RETURN 1 + "a"

ASYNC FUNCTION FailsAtOnce()
RETURN {} + 1

ASYNC FUNCTION Catcher()
   LOCAL e
   TRY
      AWAIT Fails()
   CATCH e
      RETURN "error " + e:Description + " " + e:Operation
   END
RETURN "none"

ASYNC FUNCTION SeqAcross()
   LOCAL x := "before"
   BEGIN SEQUENCE
      x := AWAIT TaskRun( {|| "in" } )
      AWAIT TaskDelay( 2 )
      Break( x + "-broken" )
   RECOVER USING x
      x := "recovered " + x
   END SEQUENCE
RETURN x

// The BREAK waits while the ALWAYS code awaits, then RECOVER takes it; without RECOVER it fails the task after
// the ALWAYS code ran.
ASYNC FUNCTION AlwaysAcross()
   LOCAL a := {}, t, v
   BEGIN SEQUENCE
      AAdd( a, 1 )
      AWAIT TaskDelay( 1 )
      AAdd( a, 2 )
      Break( "out" )
   RECOVER USING v
      AAdd( a, v )
   ALWAYS
      AAdd( a, AWAIT TaskRun( {|| 3 } ) )
   END SEQUENCE
   t := AlwaysThenFails( a )
   t:Wait()
RETURN LTrim( Str( Len( a ) ) ) + " " + a[ 3 ] + Str( a[ 4 ], 2 ) + " " + a[ 5 ] + ", task failed with " + t:Error

ASYNC FUNCTION AlwaysThenFails( a )
   BEGIN SEQUENCE
      AWAIT TaskDelay( 1 )
      Break( "unrecovered" )
   ALWAYS
      AAdd( a, AWAIT D( "always ran" ) )
   END SEQUENCE
RETURN NIL

ASYNC FUNCTION BreakCatch()
   LOCAL v
   BEGIN SEQUENCE
      v := AWAIT TaskRun( {|| Break( "bv" ) } )
   RECOVER USING v
      v := "got " + v
   END SEQUENCE
RETURN v

ASYNC FUNCTION HandlerAcross()
   LOCAL r
   BEGIN SEQUENCE WITH {| e | Break( "H:" + e:Description ) }
      AWAIT TaskDelay( 1 )
      r := 1 + "x"
   RECOVER USING r
   END SEQUENCE
RETURN r + " " + iif( AWAIT ErrorsReachDefault(), "outside handler gone", "handler kept" )

// After the sequence, an error is the task's again: the handler set inside it is not the error block any more.
ASYNC FUNCTION ErrorsReachDefault()
   LOCAL t := TaskRun( {|| 1 + "y" } )
   t:Wait()
RETURN t:IsFailed

// An error block the function sets stays its own across AWAIT, and the caller's is untouched.
ASYNC FUNCTION OwnErrorBlock()
   LOCAL r
   ErrorBlock( {| e | Break( "own " + e:Operation ) } )
   AWAIT TaskDelay( 1 )
   BEGIN SEQUENCE
      r := {} * 2
   RECOVER USING r
   END SEQUENCE
RETURN r

ASYNC FUNCTION ReturnThroughAlways()
   LOCAL a := { "body" }
   TRY
      AWAIT TaskDelay( 1 )
      RETURN a
   FINALLY
      AAdd( a, AWAIT D( "finally" ) )
   END
RETURN NIL

ASYNC FUNCTION Canceled()
   LOCAL s := CancellationTokenSource(), e
   s:CancelAfter( 5 )
   TRY
      AWAIT TaskDelay( 100000, s:Token )
   CATCH e
      RETURN "canceled " + LTrim( Str( e:SubCode ) )
   END
RETURN "not"

CLASS Acc
   VAR n INIT 0
   METHOD New() INLINE Self
   ASYNC METHOD AddLater( k )
ENDCLASS

ASYNC METHOD AddLater( k ) CLASS Acc
   AWAIT TaskDelay( 1 )
   ::n += k
RETURN ::n

ASYNC FUNCTION WithAcross( o )
   WITH OBJECT o
      AWAIT TaskDelay( 1 )
      :n += 100
   END
RETURN o:n

ASYNC PROCEDURE Proc()
   AWAIT TaskDelay( 1 )
RETURN

STATIC ASYNC FUNCTION Hidden( c )
RETURN c + AWAIT D( c )

// SET values and the current area changed in the function are its own: the caller's stay; its own persist across
// AWAIT.
ASYNC FUNCTION EnvOwn()
   LOCAL a, b, t
   SET DECIMALS TO 4
   t := Changer()
   a := "caller " + LTrim( Str( Set( _SET_DECIMALS ) ) ) + "/" + LTrim( Str( Select() ) )
   b := AWAIT t
RETURN a + ", function " + b + ", caller after " + LTrim( Str( Set( _SET_DECIMALS ) ) )

ASYNC FUNCTION Changer()
   LOCAL a
   SET DECIMALS TO 7
   SELECT 5
   a := LTrim( Str( Set( _SET_DECIMALS ) ) ) + "/" + LTrim( Str( Select() ) )
   AWAIT TaskDelay( 2 )
RETURN a + " then " + LTrim( Str( Set( _SET_DECIMALS ) ) ) + "/" + LTrim( Str( Select() ) )

// A PRIVATE of the caller is visible in every step, the same variable: the caller sees what the function assigned.
STATIC FUNCTION Privates()
   LOCAL c
   PRIVATE cShared := "caller"
   c := Assigns():Result
RETURN "private: " + c + ", caller sees " + M->cShared

ASYNC FUNCTION Assigns()
   LOCAL c := M->cShared
   AWAIT TaskDelay( 1 )
   M->cShared := "function"
RETURN c + " -> " + M->cShared

// PRIVATEs the function creates (a statement, an assignment to a name no variable has) are its own environment's:
// visible to what it calls, across AWAIT, until it ends.
ASYNC FUNCTION OwnPrivates()
   PRIVATE cMine := "mine"
   M->zz := 1
   AWAIT TaskDelay( 1 )
   M->zz++
RETURN "own privates: " + SeesPrivates() + Str( M->zz, 2 )

STATIC FUNCTION SeesPrivates()
RETURN M->cMine + "/" + LTrim( Str( M->zz ) )

// SET DATE FORMAT and SET CENTURY in the function stay its own across AWAIT; the caller's change meanwhile is not seen.
ASYNC FUNCTION DateFormats()
   LOCAL d := SToD( "20261116" ), c, t
   SET DATE FORMAT TO "dd/mm/yyyy"
   c := DToC( d )
   t := TaskDelay( 5 )
   AWAIT t
   c += " " + DToC( d )
   SET CENTURY OFF
   AWAIT TaskDelay( 1 )
RETURN "dates: " + c + " " + DToC( d )

// Tables the function opens stay open across AWAIT and close at its end; the caller's areas are untouched.
ASYNC FUNCTION Tables()
   LOCAL n
   FErase( "async_t.dbf" )
   dbCreate( "async_t.dbf", { { "N", "N", 5, 0 } } )
   USE async_t NEW EXCLUSIVE
   APPEND BLANK
   REPLACE N WITH 1
   AWAIT TaskDelay( 2 )
   APPEND BLANK
   REPLACE N WITH 2
   AWAIT TaskRun( {|| NIL } )
   n := LastRec()
   GO TOP
   n := n * 10 + FIELD->N
RETURN n

// The call stack: the caller below the first step; nothing below a later step (it runs on a worker).
ASYNC FUNCTION Names()
   LOCAL c := ProcName( 0 ) + "<" + ProcName( 1 )
   AWAIT TaskDelay( 1 )
RETURN c + ", then " + ProcName( 0 ) + "<" + ProcName( 1 )

STATIC FUNCTION ArrStr( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += LTrim( Str( x ) ) + " "
   NEXT
RETURN RTrim( c )

EXIT PROCEDURE Cleanup()
   FErase( "async_t.dbf" )
RETURN
