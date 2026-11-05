// An operation through a FOR EACH reference raises its own error before the store finds the element gone.
PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, e
   ? "start"
   FOR EACH e IN a
      ASize( a, 0 )
      e := "x" + 1
   NEXT
   RETURN
