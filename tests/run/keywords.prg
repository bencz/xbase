// Keywords are only keywords where the grammar expects them.
PROCEDURE Main()
   LOCAL next := 0, exit := 0, i
   next = 1
   exit = 2
   next += 1
   FOR i := 1 TO 3
      IF i == 2
         EXIT
      ENDIF
   NEXT
   ? next, exit, i
   DO Show WITH , 2
   RETURN

PROCEDURE Show( a, b )
   ? ValType( a ), b
   RETURN
