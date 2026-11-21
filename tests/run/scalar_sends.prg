// Sends to values of built-in types that the runtime's own classes answer, called directly where the receiver is one of
// those types and no class of the program answers first (resolve-scalar-sends), at -O0 and -O2 (run0.*): one function
// given an array, a string, a hash, a number and NIL (each its own path: Enumerable's methods, NUMERIC's, the error of
// a message nobody answers); a method a type's class overrides (ARRAY:Count, CHARACTER:Count); chains of several
// generators ended by each terminal method; a chain whose first method is no generator; ForEach with a block that
// changes the caller's variables; and ClassAssociate() while the program runs: the same sends then reach the associated
// class, the chain before it the runtime's.
PROCEDURE Main()
   LOCAL x
   FOR EACH x IN { { 3, 1, 2 }, "cab", { "k" => 5, "j" => 7 } }
      ? "chain:", Chain( x ), "count:", x:Count(), x:Count( {| v | ValType( v ) == "N" } ), "len:", Len( x )
   NEXT
   ? "number:", ( 2.567 ):Round( 1 ), Try( {|| ( 5 ):Where( {| v | v > 1 } ) } )
   ? "nil:", Try( {|| Chain( NIL ) } )
   ? "terminals:", Terminals( { 4, 8, 15, 16, 23, 42 } )
   ? "terminals of a string:", Terminals( "hello" )
   ? "not a generator first:", { 3, 1, 2 }:ToArray():Len(), "abc":Upper():Reverse():ToArray()[ 1 ]
   ? "foreach:", Spelled( "Alexandre" ), Spelled( { 10, 20 } )
   ? "before:", Chain( { 1, 2, 3 } ), Chain( "xyz" )
   ASSOCIATE CLASS Mine WITH TYPE ARRAY
   ? "after:", Chain( { 1, 2, 3 } ), Chain( "xyz" ), { 9 }:Count()
RETURN

// The same chain for whatever it is given.
FUNCTION Chain( x )
RETURN x:Where( {| v | ! Empty( v ) } ):Select( {| v | IIf( ValType( v ) == "N", v * 2, Upper( v ) ) } ):Count()

// ForEach( {| element, position | ... } ) as a loop written in place: its block changes the caller's variables.
FUNCTION Spelled( x )
   LOCAL c := "", n := 0
   x:ForEach( {| e, i | c += IIf( ValType( e ) == "C", e, LTrim( Str( e ) ) ) + Str( i, 1 ), n += i } )
RETURN c + " " + LTrim( Str( n ) )

FUNCTION Terminals( x )
   LOCAL q := {| v | ValType( v ) $ "NC" }
   RETURN LTrim( Str( x:Where( q ):Sum( {| v | IIf( ValType( v ) == "N", v, Len( v ) ) } ) ) ) + " " + ;
      Text( x:Where( q ):First() ) + " " + Text( x:Where( q ):Last() ) + " " + ;
      Text( x:Where( q ):Any() ) + " " + Text( x:Where( q ):All( {| v | ! Empty( v ) } ) ) + " " + ;
      Text( x:Select( {| v | v } ):Take( 2 ):ToArray()[ 2 ] ) + " " + Text( x:Skip( 1 ):Min() ) + " " + ;
      Text( x:Reverse():Max() ) + " " + Text( Len( x:Distinct():ToArray() ) )

STATIC FUNCTION Text( x )
   DO CASE
   CASE ValType( x ) == "C"
      RETURN x
   CASE ValType( x ) == "L"
      RETURN IIf( x, "T", "F" )
   ENDCASE
RETURN LTrim( Str( x ) )

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x

// Associated with arrays while the program runs: its Where answers them from then on.
CLASS Mine
   METHOD Where( b ) INLINE "mine"
   METHOD Count() INLINE -9
ENDCLASS
