// A string longer than memory can hold is the error BASE/9032 Memory low (the reference ends the program), on 32-bit
// hosts too (the length is checked before it is narrowed: there, the string overflow); not handled, the default error
// handler ends the program with it (tests/run/memory_low.prg handles it).
PROCEDURE Main()
   ? "before"
   ? Len( Space( 2 ^ 62 ) )
   ? "not reached"
   RETURN
