// Arithmetic and comparisons of numbers done inline (integers, doubles, both mixed; / and % with SET DECIMALS),
// results, decimals and display widths as the runtime's operators make them, with values in variables (computed at
// run time, not folded); array elements reached with a double index (truncated); integers beyond 2^53 with doubles.
PROCEDURE Main()
   LOCAL i := 7, j := -3, d := 2.5, e := 0.125, big := 9223372036854775807, z := 0.0, n
   ? i + d, d + i, i - d, d - e, d * e, i * d, e * e * e
   ? Str( i + d, 10, 3 ), Len( Str( d * e ) ), Len( Str( e * e * e ) )
   ? i * 2, i - j, j * j, big + 1, big * 2, -big - 2
   ? i < d, d < i, i = 7.0, 7.0 == i, d <> e, d >= 2.5, e <= 0.1, i > j
   n := Sqrt( -1 )
   ? n == n, n < 1, n > 1, n <= 1, n >= 1, n <> 1
   ? z == -z, 1 / 3 + 1, ( 1 / 3 ) * 3
   FOR n := 0.5 TO 2.5 STEP 0.5
      ?? n
   NEXT
   ? i / 2, i % 4, j % 2, -i % 3, i % -1, d % 1, d / e, i / d
   SET DECIMALS TO 4
   ? i / 3, d % 0.75, 2 ** 3
   SET DECIMALS TO 2
   n := { 10, 20, 30 }
   ? n[ 1.9 ], n[ 2.0 ], n[ i / 2 - 0.5 ], n[ 3.99 ]
   n[ 2.5 ] := 99
   ? n[ 2 ], Len( n )
   n := { 9007199254740995, -1.0, 9007199254740993, 9007199254740992.0 }
   ? Str( n[ 1 ] + n[ 2 ], 25, 1 ), n[ 3 ] == n[ 4 ], n[ 3 ] - n[ 4 ]
   RETURN
