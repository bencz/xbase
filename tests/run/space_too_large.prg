// A string longer than memory can hold ends the program, on 32-bit hosts too (the length is checked before it is
// narrowed).
PROCEDURE Main()
   ? "before"
   ? Len( Space( 2 ^ 62 ) )
   ? "not reached"
   RETURN
