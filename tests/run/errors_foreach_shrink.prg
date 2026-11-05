// A descending FOR EACH over an array that shrank under it raises a bound error (see .err).
PROCEDURE Main()
   LOCAL v, b := { 1, 2, 3, 4, 5 }
   FOR EACH v IN b DESCEND
      ?? v, ""
      IF v == 4
         ASize( b, 2 )
      ENDIF
   NEXT
   RETURN
