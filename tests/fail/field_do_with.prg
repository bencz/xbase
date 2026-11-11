// DO ... WITH passes a variable by reference, which a field cannot be.
FIELD X

PROCEDURE Main()
   DO Foo WITH X
RETURN

PROCEDURE Foo( a )
RETURN
