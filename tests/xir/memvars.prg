// Memory variables: mv.* operations by name; a function that may create PRIVATEs takes the activation's mark.
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
