// RANGE belongs to a RESULT: no call's argument is checked against a parameter's.
EXTERN FUNCTION Foo( a AS INTEGER RANGE 0 TO 9 ) NATIVE RESULT STRING

PROCEDURE Main()
   RETURN
