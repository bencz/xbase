// AWAIT of a call to an ASYNC function, or of a message, takes the function's result itself when its first step does
// not suspend (no task is made): checked here that it gives exactly what AWAIT of the task would, in every case. The
// function completes at once, suspends, fails at once or after suspending, returns a task (AWAIT gives that task), or
// is called among the arguments of another (only the outer call is the AWAIT's); the message reaches an ASYNC method, a
// plain method, a variable, a method of a runtime class, a generator's MoveNext; a plain function, a codeblock and a
// function reference evaluated by a native function in between are never taken for the awaited function; recursion and
// methods of Self; all at -O0 and -O2 alike (inlined entries). A request whose call unwinds (a BREAK out of a message
// a codeblock or a function reference answers) ends there: a later call nobody awaits still gets a task.
PROCEDURE Main()
   ? AWAIT Run()
RETURN

ASYNC FUNCTION Run()
   LOCAL o := Obj():New(), t, e, c := "", g
   c += "at once " + Str( AWAIT Now( 2 ), 2 )
   c += ", later " + Str( AWAIT Later( 3 ), 2 )
   c += ", nested " + Str( AWAIT Now( AWAIT Now( 4 ) ), 2 )
   c += ", argument task " + Str( AWAIT Inner( Later( 5 ) ), 2 )
   t := AWAIT GivesTask()
   c += ", task given " + ValType( t ) + Str( AWAIT t, 3 )
   TRY
      AWAIT FailsNow()
   CATCH e
      c += ", failed at once: " + e:Operation
   END
   TRY
      AWAIT FailsLater()
   CATCH e
      c += ", failed later: " + e:Operation
   END
   ? c
   c := "method " + Str( AWAIT o:Twice( 6 ), 3 )
   c += ", suspending method " + Str( AWAIT o:TwiceLater( 7 ), 3 )
   c += ", plain method " + Str( AWAIT o:Plain( 8 ), 3 )
   c += ", variable " + Str( AWAIT o:n, 3 )
   c += ", self " + Str( AWAIT o:ViaSelf( 9 ), 3 )
   c += ", runtime class " + Str( AWAIT TaskFromResult( 11 ):ContinueWith( {| x | x:Result + 1 } ), 3 )
   c += Str( AWAIT TaskDelay( 1 ):ContinueWith( {|| 10 } ), 3 )
   ? c
   g := Gen()
   c := "generator " + iif( AWAIT g:MoveNext(), "moved", "ended" ) + Str( g:Current, 2 )
   c += ", plain function " + ValType( AWAIT Plain( 1 ) )
   c += ", eval of a reference " + Str( AWAIT Eval( @Now(), 12 ), 3 )
   c += ", block " + Str( AWAIT Eval( {|| Now( 13 ) } ), 3 )
   c += ", array of tasks " + ValType( AWAIT { Now( 1 ) } )
   c += ", recursion " + Str( AWAIT Fib( 15 ), 4 )
   ? c
   c := "plain call gives a task: " + ValType( Now( 1 ) ) + " " + ValType( Later( 1 ) )
   ? c
   ? "after an unwound AWAIT:", AWAIT Unwound( o )
RETURN "done"

STATIC FUNCTION Brk()
   Break( 1 )
RETURN NIL

STATIC ASYNC FUNCTION Unwound( o )
   LOCAL f := @Brk(), b := {|| Break( 2 ) }, c := ""
   BEGIN SEQUENCE
      AWAIT f:Exec()
   END SEQUENCE
   c += ValType( o:Twice( 3 ) )
   BEGIN SEQUENCE
      AWAIT b:Eval()
   END SEQUENCE
   c += ValType( o:Twice( 3 ) )
   BEGIN SEQUENCE
      AWAIT Eval( b )
   END SEQUENCE
   c += ValType( Now( 3 ) ) + ValType( o:Twice( 3 ) )
RETURN c

ASYNC FUNCTION Now( n )
RETURN n

ASYNC FUNCTION Later( n )
   AWAIT TaskDelay( 1 )
RETURN n

ASYNC FUNCTION Inner( t )
RETURN AWAIT t

ASYNC FUNCTION GivesTask()
RETURN Later( 14 )

ASYNC FUNCTION FailsNow()
RETURN {} - 1

ASYNC FUNCTION FailsLater()
   AWAIT TaskDelay( 1 )
RETURN {} * 1

ASYNC FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
RETURN AWAIT Fib( n - 1 ) + AWAIT Fib( n - 2 )

STATIC FUNCTION Plain( n )
RETURN n

FUNCTION Gen()
   YIELD 1
RETURN

CLASS Obj
   VAR n INIT 100
   METHOD New() INLINE Self
   ASYNC METHOD Twice( k )
   ASYNC METHOD TwiceLater( k )
   METHOD Plain( k ) INLINE k * 3
   ASYNC METHOD ViaSelf( k )
ENDCLASS

ASYNC METHOD Twice( k ) CLASS Obj
RETURN k * 2

ASYNC METHOD TwiceLater( k ) CLASS Obj
   AWAIT TaskDelay( 1 )
RETURN k * 2

ASYNC METHOD ViaSelf( k ) CLASS Obj
RETURN AWAIT ::Twice( k ) + AWAIT ::TwiceLater( k )
