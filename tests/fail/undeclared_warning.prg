// A name nothing declares is a memory variable, with a warning (the EXIT outside a loop ends the compilation, so that
// the warning shows in a compile-failure test).
PROCEDURE Main()
   ? undeclared
   EXIT
RETURN
