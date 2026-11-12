// A destructor that runs while Type() evaluates its text (a collection the text's allocation makes finds the object
// unreachable): the destructor's error goes to the error block, as any destructor's does, and is not Type()'s; Type()
// tells the type of its text.
STATIC cSeen := "not run"

PROCEDURE Main()
   Drop()
   ? Type( "Len( Space( 20000000 ) )" ), cSeen
RETURN

STATIC PROCEDURE Drop()
   TDrop():New()
RETURN

CREATE CLASS TDrop
   EXPORTED:
   METHOD New
   DESTRUCTOR Gone
ENDCLASS

METHOD New() CLASS TDrop
   RETURN Self

METHOD PROCEDURE Gone() CLASS TDrop
   LOCAL bOld := ErrorBlock( {| e | Break( e ) } ), x
   BEGIN SEQUENCE
      x := 1 + "a"
      cSeen := "no error"
   RECOVER
      cSeen := "caught"
   END
   ErrorBlock( bOld )
   RETURN
