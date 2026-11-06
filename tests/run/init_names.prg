// An INIT or EXIT procedure's name is its own: a call by that name goes to the
// function of that name elsewhere, and a function of the module may share it.
PROCEDURE Main()
   ? "main"
   X()
   Foo()
RETURN

INIT PROCEDURE X()
   ? "init x"
RETURN

INIT FUNCTION Foo()
   ? "init foo"
RETURN NIL

FUNCTION Foo()
   ? "public foo"
RETURN NIL
