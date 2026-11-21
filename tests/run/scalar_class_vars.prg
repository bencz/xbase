// A SCALAR CLASS that inherits instance variables from a class of another module (which the compiler cannot see): its
// values have no area for them, so its first message is BASE/3005, as ClassAssociate() of such a class is; a type
// whose class has none keeps answering.
PROCEDURE Main()
   ? "inherits variables:", Try( {|| "x":Up() } ), Try( {|| "y":Who() } )
   ? "other types:", { 1, 2 }:Len(), ( 2.5 ):Round( 0 )
RETURN

SCALAR CLASS CHARACTER INHERIT TWithVars
   METHOD Up() INLINE Upper( Self )
ENDCLASS

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
