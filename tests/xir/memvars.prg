// Memory variables: mv.* operations by name; a function that may create PRIVATEs takes the activation's mark, and so
// does one that calls a PRIVATES function (RESTORE), in a codeblock too.
MEMVAR pub
PROCEDURE Main()
   PUBLIC pub := 1
   PRIVATE p := pub + 1
   Show( @p )
RETURN

PROCEDURE Show
   PARAMETERS a
   ? a, M->pub
RETURN

PROCEDURE Load()
   RESTORE FROM saved
   Eval( {|| __MemRestore( "saved", .T. ) } )
RETURN
