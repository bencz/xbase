// An error nobody handles in a thread: the default handler reports it and the program ends with exit code 1, the start
// procedure waiting in Join() going no further.
PROCEDURE Main()
   LOCAL oM := Mutex(), t
   oM:Lock()
   t := ThreadStart( {|| oM:Lock(), Fails( "a" ) } )
   ? "joining"
   oM:Unlock()
   t:Join()
   ? "not reached"
RETURN

STATIC FUNCTION Fails( x )
RETURN 1 + x
