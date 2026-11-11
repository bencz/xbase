// EXIT cannot leave FINALLY.
PROCEDURE Main()
   LOCAL i
   FOR i := 1 TO 3
      TRY
         ? i
      FINALLY
         EXIT
      END
   NEXT
RETURN
