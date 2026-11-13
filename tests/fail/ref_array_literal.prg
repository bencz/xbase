// A reference in an array literal is an error by design: an element never holds a reference.
PROCEDURE Main()
   LOCAL x := 1, a
   a := { @x }
   RETURN
