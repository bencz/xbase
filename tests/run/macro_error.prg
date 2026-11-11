// The default error handler on a macro that is not an expression: error 1449 on the operator `&`, the call stack,
// and exit code 1.
MEMVAR c
PROCEDURE Main()
   PRIVATE c := "1 +"
   ? "before"
   ? &c
   ? "not reached"
RETURN
