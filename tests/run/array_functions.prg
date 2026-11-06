// AScan, AEval, AIns, ADel and ACopy: ranges (a start of 0 is 1, a negative one or one past the end does nothing; a
// count of 0 does nothing, a negative one means the rest), AScan's matching (SET EXACT for strings, NIL with NIL,
// arrays and blocks never) and blocks, AEval over an array its block grows, AIns and ADel at 0, ACopy's target
// position (one array as source and target copies forward; elements are copied as references), AScan without a
// value, fractional and huge positions, and arguments that are not arrays. The last line is AEval's argument error.
PROCEDURE Main()
   LOCAL a, b, n, d := SToD( "20240229" )
   a := { 1, 2, 3, 2, "x", .T., NIL, 2.0 }
   ? AScan( a, 2 ), AScan( a, 2, 3 ), AScan( a, 2, 3, 1 ), AScan( a, 2, 5 ), AScan( a, "x" ), AScan( a, NIL )
   ?? AScan( a, 9 )
   ? AScan( a, {| x | ValType( x ) == "L" } ), AScan( a, 2, 0 ), AScan( a, 2, -1 ), AScan( a, 2, 99 )
   ?? AScan( a, 2, 2, 0 )
   ? AScan( a, 2.0 ), AScan( {}, 1 ), AScan( "abc", 1 ), AScan( { "ab", "a" }, "a" ), AScan( a, 2, NIL, 3 )
   ? AScan( { 1, 2, 3 }, 3, 1, -1 ), AScan( { 1, 2 }, {| x | 5 } ), AScan( { 1, d }, d ), AScan( { .F., .T. }, .T. )
   b := { 1 }
   ? AScan( { 5, b }, b ), AScan( { 1, 2 }, "1" ), AScan( { 1, 2, 3 }, {| x, i | i == 2 } )
   SET EXACT ON
   ? AScan( { "ab", "a" }, "a" ), AScan( { "a  " }, "a" )
   SET EXACT OFF
   ? Show( AIns( { 1, 2, 3, 4, 5 }, 2 ) ), Show( AIns( { 1, 2, 3 }, 0 ) ), Show( AIns( { 1, 2, 3 }, 3 ) )
   ?? Show( AIns( { 1, 2, 3 }, 4 ) )
   ? Show( ADel( { 1, 2, 3 }, 1 ) ), Show( ADel( { 1, 2, 3 }, 3 ) ), Show( ADel( { 1, 2, 3 }, 4 ) )
   ?? Show( ADel( { 1, 2, 3 }, 0 ) )
   ? Show( ADel( { 1, 2, 3 }, -1 ) ), Show( AIns( { 1, 2, 3 }, -1 ) ), Show( ADel( { 1, 2, 3 }, 1.9 ) ), AIns( "x", 1 )
   ?? ADel( "x", 1 )
   b := Array( 5 )
   ? Show( ACopy( { 1, 2, 3 }, b ) ), Show( ACopy( { 1, 2, 3 }, Array( 5 ), 2 ) )
   ?? Show( ACopy( { 1, 2, 3 }, Array( 5 ), 2, 1 ) )
   ? Show( ACopy( { 1, 2, 3 }, Array( 5 ), 1, 3, 4 ) ), Show( ACopy( { 1, 2, 3, 4, 5, 6 }, Array( 3 ) ) )
   ? Show( ACopy( { 1, 2 }, Array( 3 ), 1, 9, 3 ) ), Show( ACopy( { 1, 2, 3 }, Array( 3 ), 1, 0 ) )
   ? Show( ACopy( { 1, 2, 3 }, Array( 3 ), 1, -1 ) ), Show( ACopy( { 1, 2, 3 }, Array( 3 ), 1, 3, 0 ) )
   ? Show( ACopy( { 1, 2, 3 }, Array( 3 ), 1, 3, -1 ) ), Show( ACopy( { 1, 2, 3 }, Array( 3 ), -1 ) )
   ? Show( ACopy( { 1, 2, 3 }, Array( 5 ), 1, 3, 9 ) ), Show( ACopy( { 1, 2, 3 }, {}, 1, 3, 9 ) )
   ?? ACopy( { 1 }, b ) == b
   ? ACopy( "x", {} ), ACopy( {}, "x" )
   a := { 1, 2, 3, 4, 5 }
   ACopy( a, a, 1, 3, 2 )
   b := { 1, 2, 3, 4, 5 }
   ACopy( b, b, 2, 3, 1 )
   ? Show( a ), Show( b )
   ? AScan( { 1, NIL } ), AScan( { 1, NIL }, ), AScan( { 1, 2, 3 }, 3, 1.9, 2.9 ), AScan( { 1, 2, 3 }, 3, -0.5 )
   b := { 1 }
   a := ACopy( { b, 2 }, Array( 2 ) )
   ? a[ 1 ] == b, Show( ADel( { 1, 2, 3 }, 10 ^ 20 ) ), Show( AIns( { 1, 2 }, 1.5 ) ), AScan( { 1, 2 }, 2, 1, 10 ^ 20 )
   n := 0
   ? ValType( AEval( { 1, 2, 3 }, {| x, i | n += x * i } ) ), n
   n := ""
   AEval( { "a", "b", "c", "d" }, {| x | n += x }, 2, 2 )
   ?? " " + n
   n := ""
   AEval( { "a", "b", "c" }, {| x, i | n += Str( i, 1 ) }, 0 )
   ?? " " + n
   n := ""
   AEval( { "a", "b", "c" }, {| x | n += x }, 2, 0 )
   AEval( { "a", "b", "c" }, {| x | n += x }, 2, -1 )
   AEval( { "a", "b", "c" }, {| x | n += x }, 5 )
   AEval( { "a", "b", "c" }, {| x | n += x }, -1 )
   ?? " " + n
   a := { 1, 2 }
   n := 0
   AEval( a, {| x | n += x, AAdd( a, x * 10 ) } )
   ? n, Len( a )
   AEval( { 1 }, "x" )
   RETURN

FUNCTION Show( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += IIf( x == NIL, "N", IIf( ValType( x ) == "N", AllTrim( Str( x ) ), "?" ) )
   NEXT
   RETURN c
