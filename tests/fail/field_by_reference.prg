// A field cannot be passed by reference: FIELD declares X a field.
FIELD X

PROCEDURE Main()
   Foo( @X )
RETURN

PROCEDURE Foo( a )
RETURN
