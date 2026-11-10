// Memory variable rules found by review, compared with the reference: a statement's initializers all run before its
// variables exist; a function's MEMVAR wins over a file STATIC, which PRIVATE does not change; PUBLIC Clipper is .T.;
// LOCAL may follow PARAMETERS.
STATIC x := "static"

PROCEDURE Main()
   PRIVATE a := "outer"
   ErrorBlock( {| e | QOut( "err", e:subCode, e:operation ), Break() } )
   Sub()
   BEGIN SEQUENCE
      Fresh()
   END SEQUENCE
   PRIVATE x := "private"
   ? "x", x, M->x
   Other()
   PUBLIC Clipper, notClipper
   ? "clipper", M->clipper, M->notClipper
   Params( 1, 2 )
RETURN

PROCEDURE Sub()
   PRIVATE a := "new", b := M->a
   ? "sub", M->a, M->b
RETURN

PROCEDURE Fresh()
   PRIVATE c := 1, d := M->c
   ? "not reached", M->d
RETURN

PROCEDURE Other()
   MEMVAR x
   ? "other", x
RETURN

PROCEDURE Params
   PARAMETERS p, q
   LOCAL r := M->p + M->q
   ? "params", r
RETURN
