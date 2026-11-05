// ASort: mixed kinds in their fixed order, strings, a comparison block, start and count (missing, 0, too large,
// past the end), the array returned, anything else giving NIL, a block whose answer is not logical, ties kept in
// their order (a stable sort), and sorting large arrays.
PROCEDURE Main()
   LOCAL a, i, ok
   a := { "b", 3, .T., NIL, CToD( "01/02/2020" ), "a", 1, .F., CToD( "" ), 2.5, {}, {|| 1 } }
   ASort( a )
   ? "mixed:"
   FOR i := 1 TO Len( a )
      ?? " " + ValType( a[ i ] )
   NEXT
   a := { "ab", "a", "b", "", "aa", "A" }
   ASort( a )
   ? "strings:"
   FOR i := 1 TO Len( a )
      ?? " [" + a[ i ] + "]"
   NEXT
   a := { 5, 3, 9, 1, 7 }
   ASort( a, , , {| x, y | x > y } )
   ? "desc:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ]
   a := { 9, 8, 7, 6, 5, 4 }
   ASort( a, 2, 3 )
   ? "range:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ], a[ 6 ]
   a := { 3, 1, 2 }
   ? "returns same:", ASort( a ) == a, ValType( ASort( {} ) )
   a := { 4, 3, 2, 1 }
   ASort( a, 3 )
   ? "from 3:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   a := { 4, 3, 2, 1 }
   ASort( a, , 2 )
   ? "count 2:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   a := { 3, 1, 2 }
   ASort( a, 0, 10 )
   ? "start 0 count 10:", a[ 1 ], a[ 2 ], a[ 3 ]
   a := { 3, 1, 2 }
   ASort( a, 5 )
   ? "past the end:", a[ 1 ], a[ 2 ], a[ 3 ]
   ? "not array:", ValType( ASort( 5 ) )
   a := { 3, 1, 2 }
   ASort( a, , , {| x, y | "x" } )
   ? "block non logical:", a[ 1 ], a[ 2 ], a[ 3 ]
   a := { { 2, "a" }, { 1, "b" }, { 2, "c" }, { 1, "d" }, { 2, "e" } }
   ASort( a, , , {| x, y | x[ 1 ] < y[ 1 ] } )
   ? "stable:"
   FOR i := 1 TO Len( a )
      ?? " " + a[ i ][ 2 ]
   NEXT
   a := Array( 20000 )
   FOR i := 1 TO Len( a )
      a[ i ] := ( i * 7919 ) % 20011
   NEXT
   ASort( a )
   ok := .T.
   FOR i := 2 TO Len( a )
      IF a[ i - 1 ] > a[ i ]
         ok := .F.
      ENDIF
   NEXT
   ? "large:", ok, a[ 1 ], a[ Len( a ) ]
   RETURN
