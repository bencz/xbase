// The default error block evaluated with something that is not an Error object: the message it sends fails, and that
// error is reported (from inside the handler, whose own frames do not show).
PROCEDURE Main()
   ? "start"
   Eval( ErrorBlock(), "x" )
   ? "not reached"
RETURN
