// IF, WHILE, CASE, RETURN and SWITCH followed by ++ or -- start their statement; most other keywords followed by
// them are variables.
PROCEDURE Main()
   LOCAL s := 5, x := 0, exit := 1, loop := 1, for := 1, do := 1
   IF --s > 3
      ? "if dec", s
   ENDIF
   WHILE ++x < 3
      ? "while inc", x
   ENDDO
   DO CASE
   CASE --s == 3
      ? "case dec", s
   ENDCASE
   SWITCH ++x
   CASE 4
      ? "switch inc", x
   ENDSWITCH
   ? "return dec", Dec( 10 )
   exit++
   loop--
   for++
   do--
   ? "variables", exit, loop, for, do
   RETURN

FUNCTION Dec( n )
   RETURN --n
