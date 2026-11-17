// QUIT in a step that runs on a worker ends the program: the start procedure, waiting for its AWAIT, unwinds without
// going on; the EXIT procedures run, and a function still suspended never resumes.
PROCEDURE Main()
   Holder( TaskCompletionSource():Task )
   AWAIT Quitter()
   ? "not reached"
RETURN

ASYNC FUNCTION Holder( oTask )
   ? "holder suspended"
   AWAIT oTask
   ? "holder resumed"
RETURN NIL

ASYNC FUNCTION Quitter()
   AWAIT TaskDelay( 5 )
   ? "quitting"
   QUIT
RETURN NIL

EXIT PROCEDURE Bye()
   ? "exit procedure"
RETURN
