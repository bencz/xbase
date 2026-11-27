// local-tables: a LOCAL holding an array of constants (numbers, logicals, NIL) that its function only reads, t[i]
// and Len( t ), is a table (table:<slot>); not one assigned after its declaration, one whose element is assigned or
// passed by reference, one passed on, captured or returned, nor an array of strings or of arrays.
FUNCTION Tables( i )
   LOCAL a := { 1, 2, 3 }, b := { .T., NIL, -1.5 }
RETURN a[ i ] + Len( a ) + IIf( b[ i ] == NIL, 0, 1 )

FUNCTION Others( i )
   LOCAL c := { 1, 2 }, d := { 1, 2 }, e := { 1, 2 }, f := { 1, 2 }, g := { 1, 2 }, h := { "x" }, k := { { 1 } }
   c := { 3 }
   d[ 1 ] := 5
   Twice( @e[ 1 ] )
   AEval( f, {|| NIL } )
RETURN c[ 1 ] + d[ 1 ] + e[ 1 ] + Eval( {|| g[ 1 ] } ) + Len( h[ 1 ] ) + k[ 1 ][ 1 ]

FUNCTION Twice( x )
   x *= 2
RETURN x
