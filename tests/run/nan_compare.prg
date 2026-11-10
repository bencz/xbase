// NaN compares as IEEE 754 says: every comparison with it is false but `!=` (also with itself, with an integer, in
// the comparisons done in place and in those the runtime does). A FOR loop whose limit is NaN does not run, and one
// whose step is NaN ends after its first pass (the counter becomes NaN); AScan finds no NaN; ASort with NaN among
// numbers ends; Max and Min take the other operand when the first is NaN.
PROCEDURE Main()
   LOCAL z := 0.0, nan, a, i, c := 0, v := { 1.5, 1 }
   nan := Log( z ) - Log( z )
   ? "ops", nan == v[ 1 ], nan = v[ 1 ], nan != v[ 1 ], nan < v[ 1 ], nan <= v[ 1 ], nan > v[ 1 ], nan >= v[ 1 ]
   ? "self", nan == nan, nan = nan, nan != nan, nan < nan, nan <= nan, nan >= nan
   ? "int", nan == v[ 2 ], nan != v[ 2 ], v[ 2 ] < nan, v[ 2 ] >= nan
   ? "runtime", Eval( {| x, y | x == y }, nan, nan ), Eval( {| x, y | x <> y }, nan, 2 ), Do( "Less", nan, 2 )
   FOR i := 1 TO nan
      c++
   NEXT
   FOR i := 10 TO nan STEP -1
      c++
   NEXT
   FOR i := 1 TO 10 STEP nan
      c++
   NEXT
   ? "for", c
   a := { 3, nan, 1, nan, 2 }
   ? "ascan", AScan( a, nan ), AScan( a, 1 ), AScan( a, {| x | x == nan } )
   ASort( a )
   ? "asort", Len( a )
   ? "max", Max( nan, 1 ), Min( nan, 1 )
RETURN

FUNCTION Less( x, y )
RETURN x < y
