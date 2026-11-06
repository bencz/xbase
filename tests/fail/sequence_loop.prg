// LOOP cannot leave the body of a BEGIN SEQUENCE; in RECOVER it may.
PROCEDURE Main()
   LOCAL i
   FOR i := 1 TO 3
      BEGIN SEQUENCE
         BREAK
      RECOVER
         LOOP
      END
      BEGIN SEQUENCE
         LOOP
      END
   NEXT
   RETURN
