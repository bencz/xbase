// An error no handler can take, in a task, fails the task with that error instead of ending the program (as an error
// nobody handled does): a handler that returns for an error that takes no substitute (which elsewhere is the
// unrecoverable error 9001), handlers that raise errors themselves until they recurse too deep (9003), each in a
// task, an ASYNC function's step after an AWAIT too; the waiter gets the error (the innermost one for the recursion)
// and handles it, the task is failed, and the program goes on with its own handler as it was.
PROCEDURE Main()
   ? "returns:", Outcome( TaskRun( {|| ErrorBlock( {|| NIL } ), Do( "NOSUCHFUNCTION" ) } ) )
   ? "recurses:", Outcome( TaskRun( {|| ErrorBlock( {| e | Raises( e ) } ), 1 + "a" } ) )
   ? "after an await:", Outcome( Later() )
   ? "the program's handler:", Outcome( TaskRun( {|| 1 + "a" } ) )
   ? "goes on:", Try( {|| 1 + "a" } )
RETURN

ASYNC FUNCTION Later()
   AWAIT TaskYield()
   ErrorBlock( {|| NIL } )
RETURN Do( "NOSUCHFUNCTION" )

// Each handler call raises another error, the call of a function named after how deep it is (DEPTH1, ...).
STATIC FUNCTION Raises( e )
   STATIC nDepth := 0
   nDepth++
RETURN Do( "DEPTH" + LTrim( Str( nDepth ) ) )

STATIC FUNCTION Outcome( t )
   LOCAL c := Try( {|| t:Result } )
RETURN c + " failed: " + iif( t:IsFailed, "yes", "no" )

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
