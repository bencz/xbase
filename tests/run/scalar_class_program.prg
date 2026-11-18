// A program's SCALAR CLASS of a type with the runtime's of the same type: the program's methods answer first (Twice,
// and Len, which the runtime's class has too), the runtime's answer what the program's does not (Upper, Round, Count),
// whatever order the system runs the modules' registrations in; a message neither answers keeps Clipper's error; a type
// whose class only the runtime has keeps its methods; a macro message reaches them alike.
PROCEDURE Main()
   LOCAL c := "Twice"
   ? "program:", "ab":Twice(), ( 21 ):Twice(), "abc":Len(), ( 7 ):Len()
   ? "runtime:", "ab":Upper(), ( 2.567 ):Round( 1 ), "abc":Count(), { 1, 2, 3 }:Len()
   ? "macro:", "xy":&c(), ( 4 ):&c()
   ? "neither:", Try( {|| "ab":Nope() } ), Try( {|| ( 1 ):Nope() } )
RETURN

SCALAR CLASS CHARACTER
   METHOD Twice() INLINE Self + Self
   METHOD Len() INLINE -1
ENDCLASS

SCALAR CLASS NUMERIC
   METHOD Twice() INLINE Self * 2
   METHOD Len() INLINE Len( LTrim( Str( Self ) ) )
ENDCLASS

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
