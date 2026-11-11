// The default error handler on a .mem file that does not exist: the file name and the system's error code, then the
// call stack, and exit code 1.
PROCEDURE Main()
   ? "before"
   RESTORE FROM memvars_restore_missing
   ? "not reached"
RETURN
