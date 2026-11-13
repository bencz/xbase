// Hashes over a list of keys of every kind (strings that differ in case or blanks, the empty string, 1, 1.0, -0.0,
// 0, a double computed at run time, a large integer, dates, and values that cannot be keys), on a hash of seven
// keys, matching case or not, and on an empty one: element reads, HHasKey(), HPos(), HGetDef(), HDel() of each key,
// HKeyAt(), HValueAt(), HPairAt() and HDelAt() at each position, HCopy(), HEval() and HScan() over ranges,
// HSort() and HFill(). Keys come as literals, from arrays made at run time, from macros and from the command line
// (s: a string, _ for a blank, n: a number, d: a date). A result shows as text; an error as E and its code.
PROCEDURE Main( ... )
   LOCAL aKeys := {}, aText := {}, aIn := {}, c, aHash
   AAdd( aKeys, "a" )
   AAdd( aText, "'a'" )
   AAdd( aKeys, "A" )
   AAdd( aText, "'A'" )
   AAdd( aKeys, "" )
   AAdd( aText, "''" )
   AAdd( aKeys, " a" )
   AAdd( aText, "' a'" )
   AAdd( aKeys, "a " )
   AAdd( aText, "'a '" )
   AAdd( aKeys, "B" )
   AAdd( aText, "'B'" )
   AAdd( aKeys, 1 )
   AAdd( aText, "1" )
   AAdd( aKeys, 1.0 )
   AAdd( aText, "1.0" )
   AAdd( aKeys, -0.0 )
   AAdd( aText, "-0.0" )
   AAdd( aKeys, 0 )
   AAdd( aText, "0" )
   AAdd( aKeys, 2.5 )
   AAdd( aText, "2.5" )
   AAdd( aKeys, 0.1 + 0.2 )
   AAdd( aText, "0.1 + 0.2" )
   AAdd( aKeys, 1000000000000 )
   AAdd( aText, "1000000000000" )
   AAdd( aKeys, SToD( "20240229" ) )
   AAdd( aText, "SToD( '20240229' )" )
   AAdd( aKeys, SToD( "" ) )
   AAdd( aText, "SToD( '' )" )
   AAdd( aKeys, .T. )
   AAdd( aText, ".T." )
   AAdd( aKeys, NIL )
   AAdd( aText, "NIL" )
   AAdd( aKeys, {} )
   AAdd( aText, "{}" )
   FOR EACH c IN { ... }
      AAdd( aIn, Decoded( c ) )
   NEXT
   ErrorBlock( {| e | Break( e ) } )
   aHash := { Base( .T. ), Base( .F. ), { => } }
   Lookups( aHash, aKeys, aText, aIn )
   Deletes( aHash, aKeys )
   Positions( aHash )
   Ranges( aHash )
   Others( aHash )
RETURN

// The hash of seven keys, matching case or not.
STATIC FUNCTION Base( lCase )
   LOCAL h := { "a" => 1, "B" => 2, "" => 3, 1 => 4, 2.5 => 5, SToD( "20240229" ) => 6, 0 => 7 }
   HCaseMatch( h, lCase )
RETURN h

STATIC PROCEDURE Lookups( aHash, aKeys, aText, aIn )
   LOCAL h, k
   ? "literal"
   FOR EACH h IN aHash
      ? ""
      ?? Probe( h, {|| h[ "a" ] }, {|| HHasKey( h, "a" ) }, ;
         {|| HPos( h, "a" ) }, {|| HGetDef( h, "a", "d" ) } )
      ?? Probe( h, {|| h[ "A" ] }, {|| HHasKey( h, "A" ) }, ;
         {|| HPos( h, "A" ) }, {|| HGetDef( h, "A", "d" ) } )
      ?? Probe( h, {|| h[ "" ] }, {|| HHasKey( h, "" ) }, ;
         {|| HPos( h, "" ) }, {|| HGetDef( h, "", "d" ) } )
      ?? Probe( h, {|| h[ " a" ] }, {|| HHasKey( h, " a" ) }, ;
         {|| HPos( h, " a" ) }, {|| HGetDef( h, " a", "d" ) } )
      ?? Probe( h, {|| h[ "a " ] }, {|| HHasKey( h, "a " ) }, ;
         {|| HPos( h, "a " ) }, {|| HGetDef( h, "a ", "d" ) } )
      ?? Probe( h, {|| h[ "B" ] }, {|| HHasKey( h, "B" ) }, ;
         {|| HPos( h, "B" ) }, {|| HGetDef( h, "B", "d" ) } )
      ?? Probe( h, {|| h[ 1 ] }, {|| HHasKey( h, 1 ) }, ;
         {|| HPos( h, 1 ) }, {|| HGetDef( h, 1, "d" ) } )
      ?? Probe( h, {|| h[ 1.0 ] }, {|| HHasKey( h, 1.0 ) }, ;
         {|| HPos( h, 1.0 ) }, {|| HGetDef( h, 1.0, "d" ) } )
      ?? Probe( h, {|| h[ -0.0 ] }, {|| HHasKey( h, -0.0 ) }, ;
         {|| HPos( h, -0.0 ) }, {|| HGetDef( h, -0.0, "d" ) } )
      ?? Probe( h, {|| h[ 0 ] }, {|| HHasKey( h, 0 ) }, ;
         {|| HPos( h, 0 ) }, {|| HGetDef( h, 0, "d" ) } )
      ?? Probe( h, {|| h[ 2.5 ] }, {|| HHasKey( h, 2.5 ) }, ;
         {|| HPos( h, 2.5 ) }, {|| HGetDef( h, 2.5, "d" ) } )
      ?? Probe( h, {|| h[ 0.1 + 0.2 ] }, {|| HHasKey( h, 0.1 + 0.2 ) }, ;
         {|| HPos( h, 0.1 + 0.2 ) }, {|| HGetDef( h, 0.1 + 0.2, "d" ) } )
      ?? Probe( h, {|| h[ 1000000000000 ] }, {|| HHasKey( h, 1000000000000 ) }, ;
         {|| HPos( h, 1000000000000 ) }, {|| HGetDef( h, 1000000000000, "d" ) } )
      ?? Probe( h, {|| h[ SToD( "20240229" ) ] }, {|| HHasKey( h, SToD( "20240229" ) ) }, ;
         {|| HPos( h, SToD( "20240229" ) ) }, {|| HGetDef( h, SToD( "20240229" ), "d" ) } )
      ?? Probe( h, {|| h[ SToD( "" ) ] }, {|| HHasKey( h, SToD( "" ) ) }, ;
         {|| HPos( h, SToD( "" ) ) }, {|| HGetDef( h, SToD( "" ), "d" ) } )
      ?? Probe( h, {|| h[ .T. ] }, {|| HHasKey( h, .T. ) }, ;
         {|| HPos( h, .T. ) }, {|| HGetDef( h, .T., "d" ) } )
      ?? Probe( h, {|| h[ NIL ] }, {|| HHasKey( h, NIL ) }, ;
         {|| HPos( h, NIL ) }, {|| HGetDef( h, NIL, "d" ) } )
      ?? Probe( h, {|| h[ {} ] }, {|| HHasKey( h, {} ) }, ;
         {|| HPos( h, {} ) }, {|| HGetDef( h, {}, "d" ) } )
   NEXT
   ? "runtime"
   FOR EACH h IN aHash
      ? ""
      FOR EACH k IN aKeys
         ?? Probe( h, {|| h[ k ] }, {|| HHasKey( h, k ) }, {|| HPos( h, k ) }, {|| HGetDef( h, k, "d" ) } )
      NEXT
   NEXT
   ? "input"
   FOR EACH h IN aHash
      ? ""
      FOR EACH k IN aIn
         ?? Probe( h, {|| h[ k ] }, {|| HHasKey( h, k ) }, {|| HPos( h, k ) }, {|| HGetDef( h, k, "d" ) } )
      NEXT
   NEXT
   ? "macro"
   FOR EACH h IN aHash
      ? ""
      FOR EACH k IN aText
         ?? Probe( h, {|| Eval( &( "{| h | h[ " + k + " ] }" ), h ) }, ;
            {|| Eval( &( "{| h | HHasKey( h, " + k + " ) }" ), h ) }, ;
            {|| Eval( &( "{| h | HPos( h, " + k + " ) }" ), h ) }, ;
            {|| Eval( &( "{| h | HGetDef( h, " + k + ", 'd' ) }" ), h ) } )
      NEXT
   NEXT
RETURN

STATIC FUNCTION Probe( h, b1, b2, b3, b4 )
RETURN " " + Shown( b1 ) + "/" + Shown( b2 ) + "/" + Shown( b3 ) + "/" + Shown( b4 ) + "#" + ;
   LTrim( Str( Len( h ) ) )

// HDel() of each key from a copy, and the keys left.
STATIC PROCEDURE Deletes( aHash, aKeys )
   LOCAL h, k, g
   ? "deletes"
   FOR EACH h IN aHash
      ? ""
      FOR EACH k IN aKeys
         g := HClone( h )
         ?? " " + Shown( {|| HDel( g, k ) } ) + ":" + Keys( g )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE Positions( aHash )
   LOCAL h, n, g
   ? "positions"
   FOR EACH h IN aHash
      ? ""
      FOR EACH n IN { -1, 0, 1, 3, 7, 8, 2.7 }
         g := HClone( h )
         ?? " " + Shown( {|| HKeyAt( h, n ) } ) + "/" + Shown( {|| HValueAt( h, n ) } ) + "/" + ;
            Shown( {|| HPairAt( h, n ) } ) + "/" + Shown( {|| HDelAt( g, n ) } ) + ":" + Keys( g )
      NEXT
   NEXT
RETURN

// HCopy(), HEval() and HScan() over ranges (start, count).
STATIC PROCEDURE Ranges( aHash )
   LOCAL h, r, v
   ? "ranges"
   FOR EACH h IN aHash
      ? ""
      FOR EACH r IN { { NIL, NIL }, { 1, NIL }, { 2, 3 }, { 0, 2 }, { 5, 99 }, { -1, NIL }, { 3, -1 }, { 9, 1 } }
         v := {}
         ?? " " + Shown( {|| Keys( HCopy( h, { => }, r[ 1 ], r[ 2 ] ) ) } ) + "/" + ;
            Shown( {|| HEval( h, {| k, x, i | AAdd( v, i ) }, r[ 1 ], r[ 2 ] ), Len( v ) } ) + "/" + ;
            Shown( {|| HScan( h, 5, r[ 1 ], r[ 2 ] ) } ) + "/" + ;
            Shown( {|| HScan( h, {| k, x | x > 3 }, r[ 1 ], r[ 2 ] ) } )
      NEXT
   NEXT
RETURN

// HSort(), HFill(), HKeys(), HValues(), a literal with keys that are one key, and keys of several kinds.
STATIC PROCEDURE Others( aHash )
   LOCAL h, g
   ? "others"
   FOR EACH h IN aHash
      g := HClone( h )
      HSort( g )
      ? Keys( g ), Shown( {|| HValues( g ) } )
      g := HClone( h )
      HFill( g, "f" )
      ?? "", Shown( {|| HValues( g ) } ), Shown( {|| HKeys( h ) } )
   NEXT
   h := { "x" => 1, "X" => 2 }
   ? Len( h ), Keys( h )
   HCaseMatch( h, .F. )
   ?? "", Len( h ), Keys( h ), h[ "x" ]
   h := { => }
   h[ "k" ] := 1
   h[ "k" ] += 10
   h[ 3 ] := "three"
   h[ SToD( "20000101" ) ] := .T.
   ? Keys( h ), Shown( {|| HValues( h ) } )
RETURN

// The keys of a hash, in order.
STATIC FUNCTION Keys( h )
   LOCAL c := "", k
   FOR EACH k IN HKeys( h )
      c += Text( k ) + ";"
   NEXT
RETURN "{" + c + "}"

// The value a block gives, as text, or E and the code of its error.
STATIC FUNCTION Shown( b )
   LOCAL x, e
   BEGIN SEQUENCE
      x := Eval( b )
   RECOVER USING e
      RETURN "E" + LTrim( Str( e:subCode ) )
   END SEQUENCE
RETURN Text( x )

STATIC FUNCTION Text( x )
   LOCAL c, y
   SWITCH ValType( x )
   CASE "A"
      c := "{"
      FOR EACH y IN x
         c += Text( y ) + ";"
      NEXT
      RETURN c + "}"
   CASE "N"
      RETURN LTrim( Str( x ) )
   CASE "C"
      RETURN "'" + x + "'"
   CASE "L"
      RETURN iif( x, "T", "F" )
   CASE "D"
      RETURN "d" + DToS( x )
   ENDSWITCH
RETURN Lower( ValType( x ) )

// A command-line argument: s:text a string, n:text a number, d:text a date, others a value of that kind.
STATIC FUNCTION Decoded( c )
   LOCAL cKind := Left( c, 1 ), cText := SubStr( c, 3 )
   DO CASE
   CASE cKind == "s"
      RETURN StrTran( cText, "_", " " )
   CASE cKind == "n"
      RETURN Val( cText )
   CASE cKind == "d"
      RETURN SToD( cText )
   CASE cKind == "l"
      RETURN .T.
   CASE cKind == "a"
      RETURN {}
   ENDCASE
RETURN NIL
