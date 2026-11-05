// A bit function given something other than a number raises its own argument error (see errors_bits.err).
PROCEDURE Main()
   LOCAL s := "a"
   ? "start"
   ? BitAnd( s, 1 )
   RETURN
