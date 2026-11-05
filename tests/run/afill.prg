// AFill: the whole array, a range from nStart (0, missing or not a number is 1; negative or past the end fills
// nothing), nCount clipped at the end (0 or negative fills nothing), decimals truncated, no value (nothing changes),
// an explicit NIL, the same array stored twice, the result; anything but an array is an error (the last line).
PROCEDURE Main()
   LOCAL a, r
   a := { 1, 2, 3, 4, 5 }
   r := AFill( a, 0 )
   Show( a )
   ? ValType( r ), r == a
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 3 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 2, 2 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 0 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, -2 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 6 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 4, 10 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 2, 0 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 2, -1 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, NIL, 2 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, "x", 2 ) ; Show( a )
   a := { 1, 2, 3, 4, 5 } ; AFill( a, 9, 2.7, 1.9 ) ; Show( a )
   a := { 1, 2 } ; AFill( a ) ; Show( a )
   a := { 1, 2 } ; AFill( a, NIL ) ; Show( a )
   a := { { 1 }, 2 } ; AFill( a, a[ 1 ] ) ; ? a[ 1 ] == a[ 2 ]
   a := Array( 3, 2 ) ; AFill( a[ 2 ], "x" ) ; ? a[ 2, 1 ], a[ 2, 2 ], a[ 1, 1 ]
   ? ValType( AFill( {}, 1 ) )
   ? ValType( AFill( "abc", 1 ) )
   RETURN

PROCEDURE Show( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += IIf( x == NIL, "N", AllTrim( Str( x ) ) ) + " "
   NEXT
   ? c
   RETURN
