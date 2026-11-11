// alias->field cannot be passed by reference.
PROCEDURE Main()
   Foo( @T->X )
RETURN

PROCEDURE Foo( a )
RETURN
