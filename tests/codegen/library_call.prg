// A call, a function reference and a REQUEST of an xbase library's functions: its library's symbols (xbx_GREET$NAME).
EXTERN FUNCTION Twice( ... ) LIBRARY GREET
EXTERN FUNCTION Decorate( ... ) LIBRARY GREET
REQUEST Decorate

PROCEDURE Main()
   ? Twice( 2 ), Eval( @Twice(), 3 )
RETURN
