// EXIT cannot leave the body of a BEGIN SEQUENCE for a loop around it; a loop inside it may.
PROCEDURE Main()
   DO WHILE .T.
      BEGIN SEQUENCE
         DO WHILE .T.
            EXIT
         ENDDO
         EXIT
      END
   ENDDO
   RETURN
