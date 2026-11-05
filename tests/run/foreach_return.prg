// RETURN inside FOR EACH restores the loop variable for those who share it (the caller of a parameter passed by
// reference, a codeblock).
PROCEDURE Main()
   LOCAL x := "orig", a := { 10, 20 }, b
   Leave( @x, a )
   ? "param after RETURN", x
   b := Inner( a )
   ? "captured after RETURN", Eval( b )
   RETURN

PROCEDURE Leave( v, a )
   FOR EACH v IN a
      RETURN
   NEXT
   RETURN

FUNCTION Inner( a )
   LOCAL y := "saved", b := {|| y }
   FOR EACH y IN a
      IF y == 20
         RETURN b
      ENDIF
   NEXT
   RETURN b
