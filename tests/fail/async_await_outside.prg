// AWAIT outside an ASYNC function: only the procedure that may start the program waits there.
PROCEDURE Main()
   ? Helper()
RETURN

FUNCTION Helper()
RETURN AWAIT TaskRun( {|| 1 } )
