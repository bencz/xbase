PROCEDURE Main()
   LOCAL n, s, i
   FOR n := 0 TO 3
      DO CASE
      CASE n == 1
         ? "one"
      CASE n == 2
         ? "two"
      CASE n == 2
         ? "two again"
      OTHERWISE
         ? "other", n
      ENDCASE
   NEXT
   DO CASE
   ENDCASE
   DO CASE
   OTHERWISE
      ? "only otherwise"
   END CASE
   DO CASE
   CASE .F.
      ? "no"
   END
   FOR EACH s IN { "a", "b", "c", "d", "ab" }
      SWITCH s
      CASE "a"
         ? "a"
      CASE "b"
         ? "b"
      CASE "c"
         ? "c (fallthrough)"
         EXIT
      OTHERWISE
         ? "other", s
      ENDSWITCH
   NEXT
   FOR n := 1 TO 4
      SWITCH n
      CASE 1
      CASE 2
         ? "one or two"
         EXIT
      CASE -3
         ? "never"
      CASE 3
         ? "three"
         LOOP
      OTHERWISE
         ? "x", n
      END SWITCH
      ? "after", n
   NEXT
   SWITCH 1
   CASE "1"
      ? "string matched number"
   ENDSWITCH
   i := 0
   DO WHILE .T.
      i++
      SWITCH i
      CASE 3
         ? "exit switch only"
         EXIT
      ENDSWITCH
      IF i > 4
         EXIT
      ENDIF
   ENDDO
   ? i
   RETURN
