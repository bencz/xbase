// A QUIT in a destructor: the program ends after it, and the destructors still queued run at the end, whole (before
// the EXIT procedures), not cut off by the QUIT.
STATIC nRan := 0, nDone := 0

PROCEDURE Main()
   Make()
   __GcCollect()
   ? "not here"
RETURN

EXIT PROCEDURE Bye
   ? "exit procedure:", nRan, nDone
RETURN

STATIC PROCEDURE Make()
   LOCAL o := TQuit():New(), p := TWork():New(), q := TWork():New()
RETURN

CLASS TQuit
   DESTRUCTOR Stop
ENDCLASS

METHOD PROCEDURE Stop CLASS TQuit
   ? "quitting"
   QUIT
RETURN

CLASS TWork
   DESTRUCTOR Work
ENDCLASS

METHOD PROCEDURE Work CLASS TWork
   nRan++
   QOut( "work" )
   nDone++
RETURN
