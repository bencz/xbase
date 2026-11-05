// The xIR passes: a branch on a constant becomes a jump (DO WHILE .T., IF .F.), the blocks it leaves unreachable
// go, x .AND. .T. is x, x .AND. .F. and x .OR. .T. are constants (equal constants compare by value), blocks
// reached only by a jump join their predecessor.
PROCEDURE Main()
   LOCAL n := 0, lg
   DO WHILE .T.
      n++
      IF n > 3
         EXIT
      ENDIF
   ENDDO
   IF .F.
      ? "never"
   ENDIF
   WHILE n < 10 .AND. .T.
      n++
   ENDDO
   ? n
   lg := n > 5
   IF lg .AND. .F.
      ? "never"
   ENDIF
   IF lg .OR. .T.
      ? "always"
   ENDIF
   RETURN
