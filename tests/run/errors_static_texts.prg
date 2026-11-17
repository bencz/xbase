// The runtime's error objects share one text for their subsystem ("BASE") and the empty string: they act as any
// string (compared, measured, joined, a hash's key, searched), a copy that grows (+=) leaves them as they were, and the
// variable takes another value; errors one after another each read "BASE".
PROCEDURE Main()
   LOCAL e1 := Caught( {|| 1 + "a" } ), e2 := Caught( {|| Len( 5 ) } ), c, h := { => }
   ? e1:subsystem, Len( e1:subsystem ), e1:subsystem == "BASE", e1:subsystem == e2:subsystem, Upper( e1:subsystem )
   c := e1:subsystem
   c += "X"
   ? c, e1:subsystem, e2:subsystem, At( "AS", e1:subsystem ), SubStr( e1:subsystem, 2, 2 )
   h[ e1:subsystem ] := 1
   h[ "BASE" ]++
   ? h[ e2:subsystem ], Len( h )
   e1:subsystem := "MINE"
   ? e1:subsystem, e2:subsystem, Caught( {|| 1 / "x" } ):subsystem
   ? "[" + Caught( {|| __ArgError( 1234 ) } ):filename + "]", ValType( e1:filename )
RETURN

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( b )
   RECOVER USING e
   END SEQUENCE
RETURN e
