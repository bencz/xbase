// Only a variable or an element can be passed by reference, not an instance variable.
PROCEDURE Main()
   LOCAL o := ErrorNew()
   F( @o:cargo )
   RETURN

FUNCTION F( x )
   RETURN x
