// A lock belongs to the flow that takes it: a thread, or an ASYNC function while its steps run. SYNCHRONIZED around
// an AWAIT keeps the mutex while the function is suspended and releases it from whatever worker runs its end: fifty
// functions that read a counter, await, and write it back count exactly; while one holds the mutex across AWAIT, no
// other thread or task takes it (a task the same worker runs meanwhile neither); once it ends, the mutex is free; a
// function that fails or breaks inside SYNCHRONIZED after an AWAIT releases it too; nested SYNCHRONIZED of one mutex in
// one function counts its depth across AWAIT. A flow that runs inside another on the same thread (an ASYNC function's
// first step) and wants a mutex that one holds gets error 9031 instead of waiting forever; the runtime's own locks
// stay the thread's (a class whose INIT calls an ASYNC function that makes an object of another class); a thread that
// joins itself from an ASYNC function's step is told so (9018).
STATIC s_n := 0

PROCEDURE Main()
   LOCAL m := Mutex(), a := {}, k, t
   FOR k := 1 TO 50
      AAdd( a, Bump( m ) )
   NEXT
   AWAIT TaskWhenAll( a )
   ? "count:", s_n
   ? "free after:", Others( m )
   t := Hold( m, 30 )
   ? "taken by another while suspended:", Others( m )
   AWAIT t
   ? "free at the end:", Others( m )
   t := Fails( m )
   t:Wait()
   ? "failed:", t:IsFailed, "free:", Others( m )
   ? "nested:", AWAIT Nested( m ), "free:", Others( m )
   ? "held outside:", HeldOutside( m ), "free:", Others( m )
   ? "class INIT:", ValType( Outer():New():x )
   ? "joins itself:", JoinsItself()
RETURN

// The caller holds m; the ASYNC function's first step runs inside it on this thread and wants m: 9031, not a hang.
STATIC FUNCTION HeldOutside( m )
   LOCAL t
   SYNCHRONIZED m
      t := WantsIt( m )
   END
   t:Wait()
RETURN iif( t:IsFailed, LTrim( Str( t:Error:SubCode ) ), "taken" )

ASYNC FUNCTION WantsIt( m )
   SYNCHRONIZED m
      AWAIT TaskDelay( 1 )
   END
RETURN NIL

CLASS Outer
   VAR x INIT MakeInner()
   METHOD New() INLINE Self
ENDCLASS

CLASS Inner
   VAR y INIT 1
   METHOD New() INLINE Self
ENDCLASS

ASYNC FUNCTION MakeInner()
   LOCAL o := Inner():New()
RETURN o:y

STATIC FUNCTION JoinsItself()
   LOCAL a := { NIL }, t
   t := ThreadStart( {|| WaitSet( a ), AJoin( a[ 1 ] ):Result } )
   a[ 1 ] := t
RETURN t:Join()

STATIC FUNCTION WaitSet( a )
   DO WHILE a[ 1 ] == NIL
      TaskDelay( 1 ):Wait()
   ENDDO
RETURN NIL

ASYNC FUNCTION AJoin( t )
   LOCAL e
   TRY
      t:Join()
   CATCH e
      RETURN LTrim( Str( e:SubCode ) )
   END
RETURN "joined"


ASYNC FUNCTION Bump( m )
   LOCAL c
   SYNCHRONIZED m
      c := s_n
      AWAIT TaskDelay( 1 )
      s_n := c + 1
   END
RETURN NIL

ASYNC FUNCTION Hold( m, nMs )
   SYNCHRONIZED m
      AWAIT TaskDelay( nMs )
   END
RETURN NIL

ASYNC FUNCTION Fails( m )
   SYNCHRONIZED m
      AWAIT TaskDelay( 1 )
      Break( "out" )
   END
RETURN NIL

ASYNC FUNCTION Nested( m )
   LOCAL c := ""
   SYNCHRONIZED m
      SYNCHRONIZED m
         AWAIT TaskDelay( 1 )
         c += iif( Others( m ) == "no, no", "held twice", "LOST" )
      END
      AWAIT TaskDelay( 1 )
      c += iif( Others( m ) == "no, no", ", held once", ", LOST" )
   END
RETURN c

// Whether another thread, and a task, can take the mutex now.
STATIC FUNCTION Others( m )
RETURN iif( ThreadStart( {|| Tries( m ) } ):Join(), "yes", "no" ) + ", " + ;
       iif( TaskRun( {|| Tries( m ) } ):Result, "yes", "no" )

STATIC FUNCTION Tries( m )
   LOCAL l := m:TryLock()
   IF l
      m:Unlock()
   ENDIF
RETURN l
