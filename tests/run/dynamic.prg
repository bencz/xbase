// Variables are untyped: the same LOCAL can hold any type.
PROCEDURE Main()
   LOCAL I := 10
   ? I, ValType( I )
   I := 'ola mundo'
   ? I, ValType( I ), Len( I )
   I := .T.
   ? I, ValType( I )
   I := NIL
   ? I, ValType( I )
   I := 3.14
   ? I, ValType( I )
   RETURN
