// Methods of the runtime's classes that a program calls directly (resolve-scalar-sends) are not copied into it typed
// for their arguments (specialize-calls: their classes and STATICs are their module's), at -O0 and -O2 (run0.*): a
// call of ARRAY:Count next to a typed copy of the program's own (Twice, which calls itself), CHARACTER:Split and
// HASH:Count given arguments of an unexpected type (their errors, as the runtime's method raises them).
PROCEDURE Main()
   ? "beside a typed copy:", Twice( 5 ), Count( { 1, 2, 3 } )
   ? "split:", Try( {|| "a b":Split( 5 ) } ), Len( "a b c":Split( " " ) )
   ? "hash count:", Try( {|| { "k" => 1 }:Count( .T. ) } ), { "k" => 1, "l" => 2 }:Count( {| v | v > 1 } )
RETURN

FUNCTION Count( x )
RETURN x:Count( {| v | v > 1 } )

FUNCTION Twice( n )
   IF n > 100
      RETURN Twice( n - 1 )
   ENDIF
RETURN n * 2

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
