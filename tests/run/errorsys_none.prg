// An ErrorSys() that installs no error block: an error is then unrecoverable (9002), exit code 1.
PROCEDURE Main()
   ? "main"
   ? SubStr( 1, 1 )
   ? "not reached"
RETURN

PROCEDURE ErrorSys()
RETURN
