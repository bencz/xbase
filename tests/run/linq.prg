// The query methods of Enumerable on arrays, strings and generators: TakeWhile, SkipWhile, SelectMany, Concat, Zip,
// Distinct (by value or key, hashable or not), Reverse, OrderBy and OrderByDescending (stable, by key), GroupBy,
// ToHash, Last, Min, Max and Average (NIL without elements), and a lazy chain over an endless generator (YIELD ++n).
PROCEDURE Main()
   LOCAL a := { 5, 3, 8, 1, 3, 9, 2 }, p := { { "ana", 30 }, { "bia", 25 }, { "caio", 30 }, { "duda", 25 } }, h
   ? "takewhile:", Show( a:TakeWhile( {| x | x > 2 } ) ), "skipwhile:", Show( a:SkipWhile( {| x | x > 2 } ) )
   ? "selectmany:", Show( { "ab", "cd" }:SelectMany( {| s | s } ) ), "concat:", Show( { 1, 2 }:Concat( { 3 } ) )
   ? "zip:", Show( { 1, 2, 3 }:Zip( { "a", "b" }, {| n, s | s + LTrim( Str( n ) ) } ) ), ;
      Len( { 1, 2 }:Zip( "xyz" ):ToArray()[ 2 ] )
   ? "distinct:", Show( a:Distinct() ), Show( { "a", "A", "b" }:Distinct( {| s | Upper( s ) } ) ), ;
      Show( { .T., .F., .T., NIL, NIL }:Distinct() )
   ? "reverse:", Show( a:Reverse() ), "abc":Reverse():Aggregate( {| x, y | x + y } )
   ? "orderby:", Show( a:OrderBy() ), Show( a:OrderByDescending() ), ;
      Show( p:OrderBy( {| r | r[ 2 ] } ):Select( {| r | r[ 1 ] } ) ), ;
      Show( p:OrderByDescending( {| r | r[ 2 ] } ):Select( {| r | r[ 1 ] } ) )
   h := p:GroupBy( {| r | r[ 2 ] } )
   ? "groupby:", Len( h ), Len( h[ 30 ] ), h[ 25 ][ 2 ][ 1 ], h:Keys()[ 1 ]
   h := p:ToHash( {| r | r[ 1 ] }, {| r | r[ 2 ] } )
   ? "tohash:", h[ "caio" ], Len( h )
   ? "last:", a:Last(), a:Last( {| x | x < 3 } ), {}:Last()
   ? "min/max/avg:", a:Min(), a:Max(), a:Average(), p:Max( {| r | r[ 2 ] } ), {}:Min(), {}:Average()
   ? "lazy:", Show( Naturals():Where( {| n | n % 3 == 0 } ):Select( {| n | n * n } ):Take( 4 ) )
RETURN

FUNCTION Naturals()
   LOCAL n := 0
   DO WHILE .T.
      YIELD ++n
   ENDDO
RETURN

STATIC FUNCTION Show( xColl )
   LOCAL c := "", x
   FOR EACH x IN xColl
      c += IIF( ValType( x ) == "N", LTrim( Str( x ) ), IIF( ValType( x ) == "L", IIF( x, "T", "F" ), ;
         IIF( x == NIL, "U", x ) ) ) + " "
   NEXT
RETURN "[" + RTrim( c ) + "]"
