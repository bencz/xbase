// Everything below that is a "command" comes from std.ch, not the compiler.
#include "commands.ch"

PROCEDURE Main()
   LOCAL a, b, c
   STORE 7 TO a, b, c
   ? a + b + c
   ? [bracket string], GREETING
   DO WHILE a > 5
      a--
   ENDDO while a > 5
   IF a == 5
      ?? " five"
   ENDIF a == 5
   FOR b := 1 TO 2
   NEXT b
   ? b
   QUIT
   ? "not reached"
   RETURN
