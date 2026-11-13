// One function far larger than the optimizer takes whole (docs/compiler.md, code generation): its statements, of
// every kind (numbers of both kinds, strings, arrays, conditions, loops, CASEs, calls, codeblocks), are compiled
// by the passes for such functions and give what they give in a function of normal size.
PROCEDURE Main()
   LOCAL n := 0, d := 0.5, c := "", a := Array( 10 ), i, h := { => }, b := {| x | x * 3 + 1 }
   AFill( a, 0 )
   d := d * 0.75 - 42 / 8
   c := Right( c + Chr( 65 + 20 % 26 ) + Str( 20, 3 ), 40 )
   a[ 2 ] += 51 - n % 7
   IF n % 3 == 84 % 3
      n += 84
   ELSE
      c := Upper( Left( c, 84 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 7 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 10
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 69, n )
   c := iif( "N" $ c, StrTran( c, "N", "-" ), c + "N" )
   d := Round( d, 2 ) + Int( n / 47 )
   n += iif( d > n, 1, -1 ) * 75
   ? 11, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 65 ) % 100003
   d := d * 0.75 - 28 / 8
   c := Right( c + Chr( 65 + 5 % 26 ) + Str( 5, 3 ), 40 )
   a[ 3 ] += 12 - n % 7
   IF n % 3 == 56 % 3
      n += 56
   ELSE
      c := Upper( Left( c, 56 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 54 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 9
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 31, n )
   c := iif( "M" $ c, StrTran( c, "M", "-" ), c + "M" )
   d := Round( d, 2 ) + Int( n / 71 )
   n += iif( d > n, 1, -1 ) * 55
   ? 23, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 73 ) % 100003
   d := d * 0.75 - 16 / 8
   c := Right( c + Chr( 65 + 29 % 26 ) + Str( 29, 3 ), 40 )
   a[ 2 ] += 81 - n % 7
   IF n % 3 == 81 % 3
      n += 81
   ELSE
      c := Upper( Left( c, 81 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 75 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 8
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 74, n )
   c := iif( "X" $ c, StrTran( c, "X", "-" ), c + "X" )
   d := Round( d, 2 ) + Int( n / 51 )
   n += iif( d > n, 1, -1 ) * 7
   ? 35, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 6 ) % 100003
   d := d * 0.75 - 72 / 8
   c := Right( c + Chr( 65 + 18 % 26 ) + Str( 18, 3 ), 40 )
   a[ 9 ] += 38 - n % 7
   IF n % 3 == 54 % 3
      n += 54
   ELSE
      c := Upper( Left( c, 54 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 19 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k5" ] := n
   CASE n % 4 == 1
      h[ "k5" ] := Len( c )
   OTHERWISE
      n -= 70
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 16, n )
   c := iif( "W" $ c, StrTran( c, "W", "-" ), c + "W" )
   d := Round( d, 2 ) + Int( n / 40 )
   n += iif( d > n, 1, -1 ) * 72
   ? 47, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 24 ) % 100003
   d := d * 0.75 - 14 / 8
   c := Right( c + Chr( 65 + 75 % 26 ) + Str( 75, 3 ), 40 )
   a[ 5 ] += 74 - n % 7
   IF n % 3 == 82 % 3
      n += 82
   ELSE
      c := Upper( Left( c, 82 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 25 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 48
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 13, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 92 )
   n += iif( d > n, 1, -1 ) * 9
   ? 59, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 8 ) % 100003
   d := d * 0.75 - 80 / 8
   c := Right( c + Chr( 65 + 27 % 26 ) + Str( 27, 3 ), 40 )
   a[ 5 ] += 64 - n % 7
   IF n % 3 == 88 % 3
      n += 88
   ELSE
      c := Upper( Left( c, 88 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 69 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k3" ] := n
   CASE n % 4 == 1
      h[ "k3" ] := Len( c )
   OTHERWISE
      n -= 55
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 41, n )
   c := iif( "I" $ c, StrTran( c, "I", "-" ), c + "I" )
   d := Round( d, 2 ) + Int( n / 75 )
   n += iif( d > n, 1, -1 ) * 59
   ? 71, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 39 ) % 100003
   d := d * 0.75 - 32 / 8
   c := Right( c + Chr( 65 + 24 % 26 ) + Str( 24, 3 ), 40 )
   a[ 1 ] += 90 - n % 7
   IF n % 3 == 32 % 3
      n += 32
   ELSE
      c := Upper( Left( c, 32 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 11 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 74
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 39, n )
   c := iif( "Q" $ c, StrTran( c, "Q", "-" ), c + "Q" )
   d := Round( d, 2 ) + Int( n / 64 )
   n += iif( d > n, 1, -1 ) * 44
   ? 83, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 58 ) % 100003
   d := d * 0.75 - 37 / 8
   c := Right( c + Chr( 65 + 78 % 26 ) + Str( 78, 3 ), 40 )
   a[ 1 ] += 10 - n % 7
   IF n % 3 == 16 % 3
      n += 16
   ELSE
      c := Upper( Left( c, 16 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 66 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 54
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 22, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 44 )
   n += iif( d > n, 1, -1 ) * 20
   ? 95, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 54 ) % 100003
   d := d * 0.75 - 6 / 8
   c := Right( c + Chr( 65 + 86 % 26 ) + Str( 86, 3 ), 40 )
   a[ 1 ] += 10 - n % 7
   IF n % 3 == 72 % 3
      n += 72
   ELSE
      c := Upper( Left( c, 72 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 74 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 41
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 44, n )
   c := iif( "L" $ c, StrTran( c, "L", "-" ), c + "L" )
   d := Round( d, 2 ) + Int( n / 45 )
   n += iif( d > n, 1, -1 ) * 77
   ? 107, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 75 ) % 100003
   d := d * 0.75 - 59 / 8
   c := Right( c + Chr( 65 + 9 % 26 ) + Str( 9, 3 ), 40 )
   a[ 3 ] += 12 - n % 7
   IF n % 3 == 35 % 3
      n += 35
   ELSE
      c := Upper( Left( c, 35 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 61 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 90
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 86, n )
   c := iif( "J" $ c, StrTran( c, "J", "-" ), c + "J" )
   d := Round( d, 2 ) + Int( n / 8 )
   n += iif( d > n, 1, -1 ) * 94
   ? 119, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 40 ) % 100003
   d := d * 0.75 - 83 / 8
   c := Right( c + Chr( 65 + 74 % 26 ) + Str( 74, 3 ), 40 )
   a[ 9 ] += 88 - n % 7
   IF n % 3 == 58 % 3
      n += 58
   ELSE
      c := Upper( Left( c, 58 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 37 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 92
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 50, n )
   c := iif( "I" $ c, StrTran( c, "I", "-" ), c + "I" )
   d := Round( d, 2 ) + Int( n / 45 )
   n += iif( d > n, 1, -1 ) * 3
   ? 131, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 46 ) % 100003
   d := d * 0.75 - 22 / 8
   c := Right( c + Chr( 65 + 79 % 26 ) + Str( 79, 3 ), 40 )
   a[ 6 ] += 15 - n % 7
   IF n % 3 == 64 % 3
      n += 64
   ELSE
      c := Upper( Left( c, 64 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 8 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 28
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 37, n )
   c := iif( "R" $ c, StrTran( c, "R", "-" ), c + "R" )
   d := Round( d, 2 ) + Int( n / 95 )
   n += iif( d > n, 1, -1 ) * 32
   ? 143, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 51 ) % 100003
   d := d * 0.75 - 64 / 8
   c := Right( c + Chr( 65 + 11 % 26 ) + Str( 11, 3 ), 40 )
   a[ 3 ] += 22 - n % 7
   IF n % 3 == 58 % 3
      n += 58
   ELSE
      c := Upper( Left( c, 58 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 52 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 71
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 36, n )
   c := iif( "S" $ c, StrTran( c, "S", "-" ), c + "S" )
   d := Round( d, 2 ) + Int( n / 56 )
   n += iif( d > n, 1, -1 ) * 71
   ? 155, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 91 ) % 100003
   d := d * 0.75 - 54 / 8
   c := Right( c + Chr( 65 + 46 % 26 ) + Str( 46, 3 ), 40 )
   a[ 9 ] += 88 - n % 7
   IF n % 3 == 49 % 3
      n += 49
   ELSE
      c := Upper( Left( c, 49 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 30 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 20
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 11, n )
   c := iif( "X" $ c, StrTran( c, "X", "-" ), c + "X" )
   d := Round( d, 2 ) + Int( n / 20 )
   n += iif( d > n, 1, -1 ) * 30
   ? 167, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 30 ) % 100003
   d := d * 0.75 - 2 / 8
   c := Right( c + Chr( 65 + 63 % 26 ) + Str( 63, 3 ), 40 )
   a[ 7 ] += 76 - n % 7
   IF n % 3 == 24 % 3
      n += 24
   ELSE
      c := Upper( Left( c, 24 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 34 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 37
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 1, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 54 )
   n += iif( d > n, 1, -1 ) * 69
   ? 179, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 79 ) % 100003
   d := d * 0.75 - 73 / 8
   c := Right( c + Chr( 65 + 41 % 26 ) + Str( 41, 3 ), 40 )
   a[ 8 ] += 17 - n % 7
   IF n % 3 == 89 % 3
      n += 89
   ELSE
      c := Upper( Left( c, 89 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 66 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 80
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 84, n )
   c := iif( "J" $ c, StrTran( c, "J", "-" ), c + "J" )
   d := Round( d, 2 ) + Int( n / 95 )
   n += iif( d > n, 1, -1 ) * 7
   ? 191, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 88 ) % 100003
   d := d * 0.75 - 72 / 8
   c := Right( c + Chr( 65 + 51 % 26 ) + Str( 51, 3 ), 40 )
   a[ 2 ] += 51 - n % 7
   IF n % 3 == 52 % 3
      n += 52
   ELSE
      c := Upper( Left( c, 52 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 51 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 14
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 62, n )
   c := iif( "E" $ c, StrTran( c, "E", "-" ), c + "E" )
   d := Round( d, 2 ) + Int( n / 52 )
   n += iif( d > n, 1, -1 ) * 8
   ? 203, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 9 ) % 100003
   d := d * 0.75 - 27 / 8
   c := Right( c + Chr( 65 + 57 % 26 ) + Str( 57, 3 ), 40 )
   a[ 2 ] += 21 - n % 7
   IF n % 3 == 15 % 3
      n += 15
   ELSE
      c := Upper( Left( c, 15 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 44 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 77
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 7, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 1 )
   n += iif( d > n, 1, -1 ) * 73
   ? 215, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 69 ) % 100003
   d := d * 0.75 - 13 / 8
   c := Right( c + Chr( 65 + 47 % 26 ) + Str( 47, 3 ), 40 )
   a[ 10 ] += 79 - n % 7
   IF n % 3 == 4 % 3
      n += 4
   ELSE
      c := Upper( Left( c, 4 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 10 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 27
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 79, n )
   c := iif( "X" $ c, StrTran( c, "X", "-" ), c + "X" )
   d := Round( d, 2 ) + Int( n / 20 )
   n += iif( d > n, 1, -1 ) * 82
   ? 227, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 45 ) % 100003
   d := d * 0.75 - 78 / 8
   c := Right( c + Chr( 65 + 47 % 26 ) + Str( 47, 3 ), 40 )
   a[ 2 ] += 61 - n % 7
   IF n % 3 == 16 % 3
      n += 16
   ELSE
      c := Upper( Left( c, 16 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 15 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 63
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 60, n )
   c := iif( "K" $ c, StrTran( c, "K", "-" ), c + "K" )
   d := Round( d, 2 ) + Int( n / 62 )
   n += iif( d > n, 1, -1 ) * 40
   ? 239, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 19 ) % 100003
   d := d * 0.75 - 14 / 8
   c := Right( c + Chr( 65 + 96 % 26 ) + Str( 96, 3 ), 40 )
   a[ 5 ] += 44 - n % 7
   IF n % 3 == 95 % 3
      n += 95
   ELSE
      c := Upper( Left( c, 95 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 34 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 62
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 89, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 67 )
   n += iif( d > n, 1, -1 ) * 3
   ? 251, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 47 / 8
   c := Right( c + Chr( 65 + 19 % 26 ) + Str( 19, 3 ), 40 )
   a[ 10 ] += 89 - n % 7
   IF n % 3 == 70 % 3
      n += 70
   ELSE
      c := Upper( Left( c, 70 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 4 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k3" ] := n
   CASE n % 4 == 1
      h[ "k3" ] := Len( c )
   OTHERWISE
      n -= 68
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 39, n )
   c := iif( "F" $ c, StrTran( c, "F", "-" ), c + "F" )
   d := Round( d, 2 ) + Int( n / 12 )
   n += iif( d > n, 1, -1 ) * 90
   ? 263, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 67 ) % 100003
   d := d * 0.75 - 47 / 8
   c := Right( c + Chr( 65 + 22 % 26 ) + Str( 22, 3 ), 40 )
   a[ 7 ] += 46 - n % 7
   IF n % 3 == 29 % 3
      n += 29
   ELSE
      c := Upper( Left( c, 29 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 69 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k5" ] := n
   CASE n % 4 == 1
      h[ "k5" ] := Len( c )
   OTHERWISE
      n -= 70
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 65, n )
   c := iif( "R" $ c, StrTran( c, "R", "-" ), c + "R" )
   d := Round( d, 2 ) + Int( n / 82 )
   n += iif( d > n, 1, -1 ) * 29
   ? 275, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 25 ) % 100003
   d := d * 0.75 - 31 / 8
   c := Right( c + Chr( 65 + 52 % 26 ) + Str( 52, 3 ), 40 )
   a[ 6 ] += 95 - n % 7
   IF n % 3 == 30 % 3
      n += 30
   ELSE
      c := Upper( Left( c, 30 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 26 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 67
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 64, n )
   c := iif( "U" $ c, StrTran( c, "U", "-" ), c + "U" )
   d := Round( d, 2 ) + Int( n / 94 )
   n += iif( d > n, 1, -1 ) * 4
   ? 287, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 36 ) % 100003
   d := d * 0.75 - 61 / 8
   c := Right( c + Chr( 65 + 34 % 26 ) + Str( 34, 3 ), 40 )
   a[ 6 ] += 25 - n % 7
   IF n % 3 == 89 % 3
      n += 89
   ELSE
      c := Upper( Left( c, 89 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 78 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 45
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 58, n )
   c := iif( "P" $ c, StrTran( c, "P", "-" ), c + "P" )
   d := Round( d, 2 ) + Int( n / 45 )
   n += iif( d > n, 1, -1 ) * 47
   ? 299, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 29 ) % 100003
   d := d * 0.75 - 14 / 8
   c := Right( c + Chr( 65 + 30 % 26 ) + Str( 30, 3 ), 40 )
   a[ 2 ] += 61 - n % 7
   IF n % 3 == 26 % 3
      n += 26
   ELSE
      c := Upper( Left( c, 26 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 44 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 27
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 62, n )
   c := iif( "C" $ c, StrTran( c, "C", "-" ), c + "C" )
   d := Round( d, 2 ) + Int( n / 79 )
   n += iif( d > n, 1, -1 ) * 1
   ? 311, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 84 ) % 100003
   d := d * 0.75 - 45 / 8
   c := Right( c + Chr( 65 + 83 % 26 ) + Str( 83, 3 ), 40 )
   a[ 2 ] += 11 - n % 7
   IF n % 3 == 85 % 3
      n += 85
   ELSE
      c := Upper( Left( c, 85 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 16 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 50
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 92, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 26 )
   n += iif( d > n, 1, -1 ) * 62
   ? 323, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 56 ) % 100003
   d := d * 0.75 - 82 / 8
   c := Right( c + Chr( 65 + 43 % 26 ) + Str( 43, 3 ), 40 )
   a[ 3 ] += 12 - n % 7
   IF n % 3 == 93 % 3
      n += 93
   ELSE
      c := Upper( Left( c, 93 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 51 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 60
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 52, n )
   c := iif( "S" $ c, StrTran( c, "S", "-" ), c + "S" )
   d := Round( d, 2 ) + Int( n / 11 )
   n += iif( d > n, 1, -1 ) * 93
   ? 335, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 22 ) % 100003
   d := d * 0.75 - 17 / 8
   c := Right( c + Chr( 65 + 4 % 26 ) + Str( 4, 3 ), 40 )
   a[ 1 ] += 20 - n % 7
   IF n % 3 == 76 % 3
      n += 76
   ELSE
      c := Upper( Left( c, 76 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 60 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 84
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 19, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 77 )
   n += iif( d > n, 1, -1 ) * 61
   ? 347, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 45 ) % 100003
   d := d * 0.75 - 20 / 8
   c := Right( c + Chr( 65 + 71 % 26 ) + Str( 71, 3 ), 40 )
   a[ 2 ] += 71 - n % 7
   IF n % 3 == 17 % 3
      n += 17
   ELSE
      c := Upper( Left( c, 17 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 3 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 2
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 93, n )
   c := iif( "G" $ c, StrTran( c, "G", "-" ), c + "G" )
   d := Round( d, 2 ) + Int( n / 14 )
   n += iif( d > n, 1, -1 ) * 68
   ? 359, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 18 ) % 100003
   d := d * 0.75 - 56 / 8
   c := Right( c + Chr( 65 + 25 % 26 ) + Str( 25, 3 ), 40 )
   a[ 9 ] += 28 - n % 7
   IF n % 3 == 4 % 3
      n += 4
   ELSE
      c := Upper( Left( c, 4 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 33 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 28
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 38, n )
   c := iif( "N" $ c, StrTran( c, "N", "-" ), c + "N" )
   d := Round( d, 2 ) + Int( n / 31 )
   n += iif( d > n, 1, -1 ) * 76
   ? 371, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 34 ) % 100003
   d := d * 0.75 - 70 / 8
   c := Right( c + Chr( 65 + 54 % 26 ) + Str( 54, 3 ), 40 )
   a[ 8 ] += 17 - n % 7
   IF n % 3 == 8 % 3
      n += 8
   ELSE
      c := Upper( Left( c, 8 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 95 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 46
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 59, n )
   c := iif( "H" $ c, StrTran( c, "H", "-" ), c + "H" )
   d := Round( d, 2 ) + Int( n / 75 )
   n += iif( d > n, 1, -1 ) * 67
   ? 383, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 65 ) % 100003
   d := d * 0.75 - 17 / 8
   c := Right( c + Chr( 65 + 69 % 26 ) + Str( 69, 3 ), 40 )
   a[ 1 ] += 20 - n % 7
   IF n % 3 == 68 % 3
      n += 68
   ELSE
      c := Upper( Left( c, 68 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 66 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k3" ] := n
   CASE n % 4 == 1
      h[ "k3" ] := Len( c )
   OTHERWISE
      n -= 3
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 57, n )
   c := iif( "Y" $ c, StrTran( c, "Y", "-" ), c + "Y" )
   d := Round( d, 2 ) + Int( n / 78 )
   n += iif( d > n, 1, -1 ) * 1
   ? 395, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 23 ) % 100003
   d := d * 0.75 - 19 / 8
   c := Right( c + Chr( 65 + 61 % 26 ) + Str( 61, 3 ), 40 )
   a[ 1 ] += 80 - n % 7
   IF n % 3 == 93 % 3
      n += 93
   ELSE
      c := Upper( Left( c, 93 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 16 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 72
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 8, n )
   c := iif( "Q" $ c, StrTran( c, "Q", "-" ), c + "Q" )
   d := Round( d, 2 ) + Int( n / 88 )
   n += iif( d > n, 1, -1 ) * 67
   ? 407, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 72 ) % 100003
   d := d * 0.75 - 62 / 8
   c := Right( c + Chr( 65 + 14 % 26 ) + Str( 14, 3 ), 40 )
   a[ 3 ] += 72 - n % 7
   IF n % 3 == 8 % 3
      n += 8
   ELSE
      c := Upper( Left( c, 8 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 32 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 25
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 36, n )
   c := iif( "G" $ c, StrTran( c, "G", "-" ), c + "G" )
   d := Round( d, 2 ) + Int( n / 13 )
   n += iif( d > n, 1, -1 ) * 65
   ? 419, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 72 ) % 100003
   d := d * 0.75 - 4 / 8
   c := Right( c + Chr( 65 + 9 % 26 ) + Str( 9, 3 ), 40 )
   a[ 8 ] += 57 - n % 7
   IF n % 3 == 42 % 3
      n += 42
   ELSE
      c := Upper( Left( c, 42 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 79 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k0" ] := n
   CASE n % 4 == 1
      h[ "k0" ] := Len( c )
   OTHERWISE
      n -= 65
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 78, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 26 )
   n += iif( d > n, 1, -1 ) * 89
   ? 431, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 58 ) % 100003
   d := d * 0.75 - 66 / 8
   c := Right( c + Chr( 65 + 69 % 26 ) + Str( 69, 3 ), 40 )
   a[ 3 ] += 62 - n % 7
   IF n % 3 == 65 % 3
      n += 65
   ELSE
      c := Upper( Left( c, 65 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 32 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 90
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 67, n )
   c := iif( "I" $ c, StrTran( c, "I", "-" ), c + "I" )
   d := Round( d, 2 ) + Int( n / 72 )
   n += iif( d > n, 1, -1 ) * 26
   ? 443, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 18 ) % 100003
   d := d * 0.75 - 54 / 8
   c := Right( c + Chr( 65 + 16 % 26 ) + Str( 16, 3 ), 40 )
   a[ 2 ] += 51 - n % 7
   IF n % 3 == 57 % 3
      n += 57
   ELSE
      c := Upper( Left( c, 57 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 41 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 10
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 86, n )
   c := iif( "F" $ c, StrTran( c, "F", "-" ), c + "F" )
   d := Round( d, 2 ) + Int( n / 55 )
   n += iif( d > n, 1, -1 ) * 10
   ? 455, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 86 ) % 100003
   d := d * 0.75 - 39 / 8
   c := Right( c + Chr( 65 + 16 % 26 ) + Str( 16, 3 ), 40 )
   a[ 1 ] += 20 - n % 7
   IF n % 3 == 92 % 3
      n += 92
   ELSE
      c := Upper( Left( c, 92 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 83 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 85
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 47, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 33 )
   n += iif( d > n, 1, -1 ) * 18
   ? 467, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 29 ) % 100003
   d := d * 0.75 - 96 / 8
   c := Right( c + Chr( 65 + 13 % 26 ) + Str( 13, 3 ), 40 )
   a[ 2 ] += 51 - n % 7
   IF n % 3 == 63 % 3
      n += 63
   ELSE
      c := Upper( Left( c, 63 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 21 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 86
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 29, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 91 )
   n += iif( d > n, 1, -1 ) * 56
   ? 479, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 52 ) % 100003
   d := d * 0.75 - 44 / 8
   c := Right( c + Chr( 65 + 54 % 26 ) + Str( 54, 3 ), 40 )
   a[ 7 ] += 26 - n % 7
   IF n % 3 == 46 % 3
      n += 46
   ELSE
      c := Upper( Left( c, 46 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 41 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 12
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 93, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 3 )
   n += iif( d > n, 1, -1 ) * 44
   ? 491, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 59 ) % 100003
   d := d * 0.75 - 57 / 8
   c := Right( c + Chr( 65 + 91 % 26 ) + Str( 91, 3 ), 40 )
   a[ 4 ] += 3 - n % 7
   IF n % 3 == 50 % 3
      n += 50
   ELSE
      c := Upper( Left( c, 50 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 43 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 67
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 80, n )
   c := iif( "M" $ c, StrTran( c, "M", "-" ), c + "M" )
   d := Round( d, 2 ) + Int( n / 66 )
   n += iif( d > n, 1, -1 ) * 9
   ? 503, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 30 ) % 100003
   d := d * 0.75 - 14 / 8
   c := Right( c + Chr( 65 + 11 % 26 ) + Str( 11, 3 ), 40 )
   a[ 5 ] += 34 - n % 7
   IF n % 3 == 35 % 3
      n += 35
   ELSE
      c := Upper( Left( c, 35 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 6 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 24
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 35, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 17 )
   n += iif( d > n, 1, -1 ) * 55
   ? 515, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 34 ) % 100003
   d := d * 0.75 - 52 / 8
   c := Right( c + Chr( 65 + 20 % 26 ) + Str( 20, 3 ), 40 )
   a[ 10 ] += 69 - n % 7
   IF n % 3 == 66 % 3
      n += 66
   ELSE
      c := Upper( Left( c, 66 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 74 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 64
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 90, n )
   c := iif( "Q" $ c, StrTran( c, "Q", "-" ), c + "Q" )
   d := Round( d, 2 ) + Int( n / 12 )
   n += iif( d > n, 1, -1 ) * 36
   ? 527, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 89 ) % 100003
   d := d * 0.75 - 24 / 8
   c := Right( c + Chr( 65 + 55 % 26 ) + Str( 55, 3 ), 40 )
   a[ 1 ] += 10 - n % 7
   IF n % 3 == 35 % 3
      n += 35
   ELSE
      c := Upper( Left( c, 35 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 3 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k4" ] := n
   CASE n % 4 == 1
      h[ "k4" ] := Len( c )
   OTHERWISE
      n -= 82
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 12, n )
   c := iif( "I" $ c, StrTran( c, "I", "-" ), c + "I" )
   d := Round( d, 2 ) + Int( n / 11 )
   n += iif( d > n, 1, -1 ) * 78
   ? 539, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 9 ) % 100003
   d := d * 0.75 - 34 / 8
   c := Right( c + Chr( 65 + 16 % 26 ) + Str( 16, 3 ), 40 )
   a[ 10 ] += 59 - n % 7
   IF n % 3 == 2 % 3
      n += 2
   ELSE
      c := Upper( Left( c, 2 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 44 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 71
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 54, n )
   c := iif( "J" $ c, StrTran( c, "J", "-" ), c + "J" )
   d := Round( d, 2 ) + Int( n / 80 )
   n += iif( d > n, 1, -1 ) * 17
   ? 551, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 91 / 8
   c := Right( c + Chr( 65 + 31 % 26 ) + Str( 31, 3 ), 40 )
   a[ 6 ] += 15 - n % 7
   IF n % 3 == 21 % 3
      n += 21
   ELSE
      c := Upper( Left( c, 21 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 34 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 7
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 24, n )
   c := iif( "A" $ c, StrTran( c, "A", "-" ), c + "A" )
   d := Round( d, 2 ) + Int( n / 40 )
   n += iif( d > n, 1, -1 ) * 81
   ? 563, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 27 / 8
   c := Right( c + Chr( 65 + 38 % 26 ) + Str( 38, 3 ), 40 )
   a[ 9 ] += 58 - n % 7
   IF n % 3 == 65 % 3
      n += 65
   ELSE
      c := Upper( Left( c, 65 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 87 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 23
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 35, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 3 )
   n += iif( d > n, 1, -1 ) * 33
   ? 575, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 2 ) % 100003
   d := d * 0.75 - 3 / 8
   c := Right( c + Chr( 65 + 94 % 26 ) + Str( 94, 3 ), 40 )
   a[ 6 ] += 65 - n % 7
   IF n % 3 == 71 % 3
      n += 71
   ELSE
      c := Upper( Left( c, 71 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 25 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 66
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 61, n )
   c := iif( "G" $ c, StrTran( c, "G", "-" ), c + "G" )
   d := Round( d, 2 ) + Int( n / 58 )
   n += iif( d > n, 1, -1 ) * 14
   ? 587, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 84 ) % 100003
   d := d * 0.75 - 56 / 8
   c := Right( c + Chr( 65 + 85 % 26 ) + Str( 85, 3 ), 40 )
   a[ 5 ] += 64 - n % 7
   IF n % 3 == 70 % 3
      n += 70
   ELSE
      c := Upper( Left( c, 70 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 51 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k0" ] := n
   CASE n % 4 == 1
      h[ "k0" ] := Len( c )
   OTHERWISE
      n -= 65
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 40, n )
   c := iif( "L" $ c, StrTran( c, "L", "-" ), c + "L" )
   d := Round( d, 2 ) + Int( n / 28 )
   n += iif( d > n, 1, -1 ) * 30
   ? 599, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 26 ) % 100003
   d := d * 0.75 - 91 / 8
   c := Right( c + Chr( 65 + 94 % 26 ) + Str( 94, 3 ), 40 )
   a[ 3 ] += 82 - n % 7
   IF n % 3 == 18 % 3
      n += 18
   ELSE
      c := Upper( Left( c, 18 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 52 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 45
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 7, n )
   c := iif( "R" $ c, StrTran( c, "R", "-" ), c + "R" )
   d := Round( d, 2 ) + Int( n / 2 )
   n += iif( d > n, 1, -1 ) * 10
   ? 611, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 95 ) % 100003
   d := d * 0.75 - 33 / 8
   c := Right( c + Chr( 65 + 56 % 26 ) + Str( 56, 3 ), 40 )
   a[ 2 ] += 21 - n % 7
   IF n % 3 == 8 % 3
      n += 8
   ELSE
      c := Upper( Left( c, 8 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 11 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 86
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 49, n )
   c := iif( "N" $ c, StrTran( c, "N", "-" ), c + "N" )
   d := Round( d, 2 ) + Int( n / 86 )
   n += iif( d > n, 1, -1 ) * 37
   ? 623, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 32 ) % 100003
   d := d * 0.75 - 89 / 8
   c := Right( c + Chr( 65 + 38 % 26 ) + Str( 38, 3 ), 40 )
   a[ 7 ] += 6 - n % 7
   IF n % 3 == 59 % 3
      n += 59
   ELSE
      c := Upper( Left( c, 59 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 24 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 21
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 35, n )
   c := iif( "G" $ c, StrTran( c, "G", "-" ), c + "G" )
   d := Round( d, 2 ) + Int( n / 1 )
   n += iif( d > n, 1, -1 ) * 34
   ? 635, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 43 ) % 100003
   d := d * 0.75 - 71 / 8
   c := Right( c + Chr( 65 + 42 % 26 ) + Str( 42, 3 ), 40 )
   a[ 3 ] += 32 - n % 7
   IF n % 3 == 5 % 3
      n += 5
   ELSE
      c := Upper( Left( c, 5 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 40 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 28
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 46, n )
   c := iif( "Y" $ c, StrTran( c, "Y", "-" ), c + "Y" )
   d := Round( d, 2 ) + Int( n / 1 )
   n += iif( d > n, 1, -1 ) * 43
   ? 647, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 11 ) % 100003
   d := d * 0.75 - 61 / 8
   c := Right( c + Chr( 65 + 36 % 26 ) + Str( 36, 3 ), 40 )
   a[ 6 ] += 65 - n % 7
   IF n % 3 == 84 % 3
      n += 84
   ELSE
      c := Upper( Left( c, 84 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 26 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 32
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 65, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 12 )
   n += iif( d > n, 1, -1 ) * 34
   ? 659, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 19 ) % 100003
   d := d * 0.75 - 52 / 8
   c := Right( c + Chr( 65 + 76 % 26 ) + Str( 76, 3 ), 40 )
   a[ 7 ] += 6 - n % 7
   IF n % 3 == 51 % 3
      n += 51
   ELSE
      c := Upper( Left( c, 51 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 3 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k0" ] := n
   CASE n % 4 == 1
      h[ "k0" ] := Len( c )
   OTHERWISE
      n -= 39
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 39, n )
   c := iif( "D" $ c, StrTran( c, "D", "-" ), c + "D" )
   d := Round( d, 2 ) + Int( n / 30 )
   n += iif( d > n, 1, -1 ) * 11
   ? 671, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 97 / 8
   c := Right( c + Chr( 65 + 20 % 26 ) + Str( 20, 3 ), 40 )
   a[ 6 ] += 85 - n % 7
   IF n % 3 == 92 % 3
      n += 92
   ELSE
      c := Upper( Left( c, 92 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 77 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 50
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 42, n )
   c := iif( "P" $ c, StrTran( c, "P", "-" ), c + "P" )
   d := Round( d, 2 ) + Int( n / 64 )
   n += iif( d > n, 1, -1 ) * 20
   ? 683, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 93 ) % 100003
   d := d * 0.75 - 80 / 8
   c := Right( c + Chr( 65 + 83 % 26 ) + Str( 83, 3 ), 40 )
   a[ 10 ] += 19 - n % 7
   IF n % 3 == 6 % 3
      n += 6
   ELSE
      c := Upper( Left( c, 6 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 92 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 66
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 81, n )
   c := iif( "D" $ c, StrTran( c, "D", "-" ), c + "D" )
   d := Round( d, 2 ) + Int( n / 94 )
   n += iif( d > n, 1, -1 ) * 90
   ? 695, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 18 ) % 100003
   d := d * 0.75 - 68 / 8
   c := Right( c + Chr( 65 + 97 % 26 ) + Str( 97, 3 ), 40 )
   a[ 6 ] += 65 - n % 7
   IF n % 3 == 73 % 3
      n += 73
   ELSE
      c := Upper( Left( c, 73 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 3 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 88
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 75, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 88 )
   n += iif( d > n, 1, -1 ) * 89
   ? 707, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 30 ) % 100003
   d := d * 0.75 - 11 / 8
   c := Right( c + Chr( 65 + 4 % 26 ) + Str( 4, 3 ), 40 )
   a[ 7 ] += 6 - n % 7
   IF n % 3 == 18 % 3
      n += 18
   ELSE
      c := Upper( Left( c, 18 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 82 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 47
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 14, n )
   c := iif( "X" $ c, StrTran( c, "X", "-" ), c + "X" )
   d := Round( d, 2 ) + Int( n / 58 )
   n += iif( d > n, 1, -1 ) * 72
   ? 719, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 81 ) % 100003
   d := d * 0.75 - 3 / 8
   c := Right( c + Chr( 65 + 81 % 26 ) + Str( 81, 3 ), 40 )
   a[ 10 ] += 69 - n % 7
   IF n % 3 == 88 % 3
      n += 88
   ELSE
      c := Upper( Left( c, 88 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 32 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 63
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 34, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 59 )
   n += iif( d > n, 1, -1 ) * 9
   ? 731, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 65 ) % 100003
   d := d * 0.75 - 69 / 8
   c := Right( c + Chr( 65 + 12 % 26 ) + Str( 12, 3 ), 40 )
   a[ 6 ] += 85 - n % 7
   IF n % 3 == 68 % 3
      n += 68
   ELSE
      c := Upper( Left( c, 68 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 9 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k5" ] := n
   CASE n % 4 == 1
      h[ "k5" ] := Len( c )
   OTHERWISE
      n -= 96
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 95, n )
   c := iif( "J" $ c, StrTran( c, "J", "-" ), c + "J" )
   d := Round( d, 2 ) + Int( n / 33 )
   n += iif( d > n, 1, -1 ) * 10
   ? 743, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 31 ) % 100003
   d := d * 0.75 - 94 / 8
   c := Right( c + Chr( 65 + 97 % 26 ) + Str( 97, 3 ), 40 )
   a[ 8 ] += 27 - n % 7
   IF n % 3 == 30 % 3
      n += 30
   ELSE
      c := Upper( Left( c, 30 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 95 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 84
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 59, n )
   c := iif( "M" $ c, StrTran( c, "M", "-" ), c + "M" )
   d := Round( d, 2 ) + Int( n / 49 )
   n += iif( d > n, 1, -1 ) * 10
   ? 755, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 88 ) % 100003
   d := d * 0.75 - 37 / 8
   c := Right( c + Chr( 65 + 6 % 26 ) + Str( 6, 3 ), 40 )
   a[ 10 ] += 79 - n % 7
   IF n % 3 == 81 % 3
      n += 81
   ELSE
      c := Upper( Left( c, 81 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 83 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k0" ] := n
   CASE n % 4 == 1
      h[ "k0" ] := Len( c )
   OTHERWISE
      n -= 26
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 10, n )
   c := iif( "Z" $ c, StrTran( c, "Z", "-" ), c + "Z" )
   d := Round( d, 2 ) + Int( n / 19 )
   n += iif( d > n, 1, -1 ) * 43
   ? 767, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 84 ) % 100003
   d := d * 0.75 - 96 / 8
   c := Right( c + Chr( 65 + 89 % 26 ) + Str( 89, 3 ), 40 )
   a[ 10 ] += 39 - n % 7
   IF n % 3 == 80 % 3
      n += 80
   ELSE
      c := Upper( Left( c, 80 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 73 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k5" ] := n
   CASE n % 4 == 1
      h[ "k5" ] := Len( c )
   OTHERWISE
      n -= 18
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 2, n )
   c := iif( "K" $ c, StrTran( c, "K", "-" ), c + "K" )
   d := Round( d, 2 ) + Int( n / 8 )
   n += iif( d > n, 1, -1 ) * 63
   ? 779, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 87 ) % 100003
   d := d * 0.75 - 13 / 8
   c := Right( c + Chr( 65 + 89 % 26 ) + Str( 89, 3 ), 40 )
   a[ 9 ] += 28 - n % 7
   IF n % 3 == 87 % 3
      n += 87
   ELSE
      c := Upper( Left( c, 87 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 63 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 38
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 91, n )
   c := iif( "P" $ c, StrTran( c, "P", "-" ), c + "P" )
   d := Round( d, 2 ) + Int( n / 37 )
   n += iif( d > n, 1, -1 ) * 60
   ? 791, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 60 ) % 100003
   d := d * 0.75 - 16 / 8
   c := Right( c + Chr( 65 + 71 % 26 ) + Str( 71, 3 ), 40 )
   a[ 7 ] += 26 - n % 7
   IF n % 3 == 40 % 3
      n += 40
   ELSE
      c := Upper( Left( c, 40 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 11 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 61
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 3, n )
   c := iif( "M" $ c, StrTran( c, "M", "-" ), c + "M" )
   d := Round( d, 2 ) + Int( n / 59 )
   n += iif( d > n, 1, -1 ) * 10
   ? 803, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 58 ) % 100003
   d := d * 0.75 - 35 / 8
   c := Right( c + Chr( 65 + 50 % 26 ) + Str( 50, 3 ), 40 )
   a[ 8 ] += 27 - n % 7
   IF n % 3 == 27 % 3
      n += 27
   ELSE
      c := Upper( Left( c, 27 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 10 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 75
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 12, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 96 )
   n += iif( d > n, 1, -1 ) * 68
   ? 815, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 47 ) % 100003
   d := d * 0.75 - 17 / 8
   c := Right( c + Chr( 65 + 78 % 26 ) + Str( 78, 3 ), 40 )
   a[ 2 ] += 81 - n % 7
   IF n % 3 == 66 % 3
      n += 66
   ELSE
      c := Upper( Left( c, 66 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 36 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 15
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 91, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 30 )
   n += iif( d > n, 1, -1 ) * 64
   ? 827, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 51 ) % 100003
   d := d * 0.75 - 4 / 8
   c := Right( c + Chr( 65 + 21 % 26 ) + Str( 21, 3 ), 40 )
   a[ 2 ] += 1 - n % 7
   IF n % 3 == 63 % 3
      n += 63
   ELSE
      c := Upper( Left( c, 63 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 88 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 58
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 52, n )
   c := iif( "N" $ c, StrTran( c, "N", "-" ), c + "N" )
   d := Round( d, 2 ) + Int( n / 94 )
   n += iif( d > n, 1, -1 ) * 19
   ? 839, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 45 ) % 100003
   d := d * 0.75 - 49 / 8
   c := Right( c + Chr( 65 + 41 % 26 ) + Str( 41, 3 ), 40 )
   a[ 7 ] += 16 - n % 7
   IF n % 3 == 43 % 3
      n += 43
   ELSE
      c := Upper( Left( c, 43 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 1 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k3" ] := n
   CASE n % 4 == 1
      h[ "k3" ] := Len( c )
   OTHERWISE
      n -= 42
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 97, n )
   c := iif( "S" $ c, StrTran( c, "S", "-" ), c + "S" )
   d := Round( d, 2 ) + Int( n / 51 )
   n += iif( d > n, 1, -1 ) * 16
   ? 851, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 92 ) % 100003
   d := d * 0.75 - 2 / 8
   c := Right( c + Chr( 65 + 95 % 26 ) + Str( 95, 3 ), 40 )
   a[ 9 ] += 38 - n % 7
   IF n % 3 == 33 % 3
      n += 33
   ELSE
      c := Upper( Left( c, 33 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 48 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 9
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 51, n )
   c := iif( "Y" $ c, StrTran( c, "Y", "-" ), c + "Y" )
   d := Round( d, 2 ) + Int( n / 76 )
   n += iif( d > n, 1, -1 ) * 10
   ? 863, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 55 ) % 100003
   d := d * 0.75 - 97 / 8
   c := Right( c + Chr( 65 + 36 % 26 ) + Str( 36, 3 ), 40 )
   a[ 8 ] += 7 - n % 7
   IF n % 3 == 36 % 3
      n += 36
   ELSE
      c := Upper( Left( c, 36 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 14 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 7
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 85, n )
   c := iif( "L" $ c, StrTran( c, "L", "-" ), c + "L" )
   d := Round( d, 2 ) + Int( n / 82 )
   n += iif( d > n, 1, -1 ) * 20
   ? 875, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 35 ) % 100003
   d := d * 0.75 - 56 / 8
   c := Right( c + Chr( 65 + 66 % 26 ) + Str( 66, 3 ), 40 )
   a[ 2 ] += 41 - n % 7
   IF n % 3 == 25 % 3
      n += 25
   ELSE
      c := Upper( Left( c, 25 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 48 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k3" ] := n
   CASE n % 4 == 1
      h[ "k3" ] := Len( c )
   OTHERWISE
      n -= 55
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 4, n )
   c := iif( "D" $ c, StrTran( c, "D", "-" ), c + "D" )
   d := Round( d, 2 ) + Int( n / 52 )
   n += iif( d > n, 1, -1 ) * 71
   ? 887, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 27 ) % 100003
   d := d * 0.75 - 93 / 8
   c := Right( c + Chr( 65 + 11 % 26 ) + Str( 11, 3 ), 40 )
   a[ 8 ] += 7 - n % 7
   IF n % 3 == 94 % 3
      n += 94
   ELSE
      c := Upper( Left( c, 94 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 53 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k6" ] := n
   CASE n % 4 == 1
      h[ "k6" ] := Len( c )
   OTHERWISE
      n -= 58
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 79, n )
   c := iif( "T" $ c, StrTran( c, "T", "-" ), c + "T" )
   d := Round( d, 2 ) + Int( n / 18 )
   n += iif( d > n, 1, -1 ) * 83
   ? 899, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 63 ) % 100003
   d := d * 0.75 - 7 / 8
   c := Right( c + Chr( 65 + 71 % 26 ) + Str( 71, 3 ), 40 )
   a[ 8 ] += 17 - n % 7
   IF n % 3 == 22 % 3
      n += 22
   ELSE
      c := Upper( Left( c, 22 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 61 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 54
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 44, n )
   c := iif( "L" $ c, StrTran( c, "L", "-" ), c + "L" )
   d := Round( d, 2 ) + Int( n / 39 )
   n += iif( d > n, 1, -1 ) * 33
   ? 911, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 95 ) % 100003
   d := d * 0.75 - 84 / 8
   c := Right( c + Chr( 65 + 34 % 26 ) + Str( 34, 3 ), 40 )
   a[ 3 ] += 52 - n % 7
   IF n % 3 == 84 % 3
      n += 84
   ELSE
      c := Upper( Left( c, 84 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 31 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k0" ] := n
   CASE n % 4 == 1
      h[ "k0" ] := Len( c )
   OTHERWISE
      n -= 39
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 62, n )
   c := iif( "U" $ c, StrTran( c, "U", "-" ), c + "U" )
   d := Round( d, 2 ) + Int( n / 86 )
   n += iif( d > n, 1, -1 ) * 51
   ? 923, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 22 ) % 100003
   d := d * 0.75 - 83 / 8
   c := Right( c + Chr( 65 + 21 % 26 ) + Str( 21, 3 ), 40 )
   a[ 1 ] += 10 - n % 7
   IF n % 3 == 27 % 3
      n += 27
   ELSE
      c := Upper( Left( c, 27 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 65 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 64
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 71, n )
   c := iif( "D" $ c, StrTran( c, "D", "-" ), c + "D" )
   d := Round( d, 2 ) + Int( n / 58 )
   n += iif( d > n, 1, -1 ) * 43
   ? 935, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 55 ) % 100003
   d := d * 0.75 - 18 / 8
   c := Right( c + Chr( 65 + 71 % 26 ) + Str( 71, 3 ), 40 )
   a[ 6 ] += 25 - n % 7
   IF n % 3 == 32 % 3
      n += 32
   ELSE
      c := Upper( Left( c, 32 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 12 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 23
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 44, n )
   c := iif( "U" $ c, StrTran( c, "U", "-" ), c + "U" )
   d := Round( d, 2 ) + Int( n / 12 )
   n += iif( d > n, 1, -1 ) * 41
   ? 947, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 48 ) % 100003
   d := d * 0.75 - 34 / 8
   c := Right( c + Chr( 65 + 73 % 26 ) + Str( 73, 3 ), 40 )
   a[ 7 ] += 26 - n % 7
   IF n % 3 == 3 % 3
      n += 3
   ELSE
      c := Upper( Left( c, 3 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 96 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 53
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 50, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 96 )
   n += iif( d > n, 1, -1 ) * 68
   ? 959, n, Str( d, 18, 4 ), c, a[ 8 ], Len( h )
   n := ( n * 31 + 49 ) % 100003
   d := d * 0.75 - 35 / 8
   c := Right( c + Chr( 65 + 44 % 26 ) + Str( 44, 3 ), 40 )
   a[ 8 ] += 97 - n % 7
   IF n % 3 == 8 % 3
      n += 8
   ELSE
      c := Upper( Left( c, 8 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 64 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k10" ] := n
   CASE n % 4 == 1
      h[ "k10" ] := Len( c )
   OTHERWISE
      n -= 36
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 74, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 17 )
   n += iif( d > n, 1, -1 ) * 88
   ? 971, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 81 / 8
   c := Right( c + Chr( 65 + 28 % 26 ) + Str( 28, 3 ), 40 )
   a[ 3 ] += 12 - n % 7
   IF n % 3 == 35 % 3
      n += 35
   ELSE
      c := Upper( Left( c, 35 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 32 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 50
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 52, n )
   c := iif( "F" $ c, StrTran( c, "F", "-" ), c + "F" )
   d := Round( d, 2 ) + Int( n / 58 )
   n += iif( d > n, 1, -1 ) * 56
   ? 983, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 3 ) % 100003
   d := d * 0.75 - 17 / 8
   c := Right( c + Chr( 65 + 5 % 26 ) + Str( 5, 3 ), 40 )
   a[ 6 ] += 55 - n % 7
   IF n % 3 == 91 % 3
      n += 91
   ELSE
      c := Upper( Left( c, 91 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 61 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 76
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 63, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 10 )
   n += iif( d > n, 1, -1 ) * 51
   ? 995, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 60 ) % 100003
   d := d * 0.75 - 58 / 8
   c := Right( c + Chr( 65 + 32 % 26 ) + Str( 32, 3 ), 40 )
   a[ 5 ] += 14 - n % 7
   IF n % 3 == 29 % 3
      n += 29
   ELSE
      c := Upper( Left( c, 29 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 20 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 20
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 67, n )
   c := iif( "K" $ c, StrTran( c, "K", "-" ), c + "K" )
   d := Round( d, 2 ) + Int( n / 14 )
   n += iif( d > n, 1, -1 ) * 93
   ? 1007, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 83 ) % 100003
   d := d * 0.75 - 59 / 8
   c := Right( c + Chr( 65 + 11 % 26 ) + Str( 11, 3 ), 40 )
   a[ 2 ] += 71 - n % 7
   IF n % 3 == 6 % 3
      n += 6
   ELSE
      c := Upper( Left( c, 6 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 1 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k4" ] := n
   CASE n % 4 == 1
      h[ "k4" ] := Len( c )
   OTHERWISE
      n -= 17
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 30, n )
   c := iif( "V" $ c, StrTran( c, "V", "-" ), c + "V" )
   d := Round( d, 2 ) + Int( n / 5 )
   n += iif( d > n, 1, -1 ) * 83
   ? 1019, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 39 ) % 100003
   d := d * 0.75 - 17 / 8
   c := Right( c + Chr( 65 + 81 % 26 ) + Str( 81, 3 ), 40 )
   a[ 4 ] += 33 - n % 7
   IF n % 3 == 68 % 3
      n += 68
   ELSE
      c := Upper( Left( c, 68 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 82 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k4" ] := n
   CASE n % 4 == 1
      h[ "k4" ] := Len( c )
   OTHERWISE
      n -= 56
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 90, n )
   c := iif( "P" $ c, StrTran( c, "P", "-" ), c + "P" )
   d := Round( d, 2 ) + Int( n / 13 )
   n += iif( d > n, 1, -1 ) * 10
   ? 1031, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 68 ) % 100003
   d := d * 0.75 - 75 / 8
   c := Right( c + Chr( 65 + 25 % 26 ) + Str( 25, 3 ), 40 )
   a[ 1 ] += 50 - n % 7
   IF n % 3 == 34 % 3
      n += 34
   ELSE
      c := Upper( Left( c, 34 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 29 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 77
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 1, n )
   c := iif( "C" $ c, StrTran( c, "C", "-" ), c + "C" )
   d := Round( d, 2 ) + Int( n / 69 )
   n += iif( d > n, 1, -1 ) * 39
   ? 1043, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 36 ) % 100003
   d := d * 0.75 - 41 / 8
   c := Right( c + Chr( 65 + 83 % 26 ) + Str( 83, 3 ), 40 )
   a[ 3 ] += 32 - n % 7
   IF n % 3 == 61 % 3
      n += 61
   ELSE
      c := Upper( Left( c, 61 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 68 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k5" ] := n
   CASE n % 4 == 1
      h[ "k5" ] := Len( c )
   OTHERWISE
      n -= 31
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 71, n )
   c := iif( "G" $ c, StrTran( c, "G", "-" ), c + "G" )
   d := Round( d, 2 ) + Int( n / 4 )
   n += iif( d > n, 1, -1 ) * 53
   ? 1055, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 84 ) % 100003
   d := d * 0.75 - 40 / 8
   c := Right( c + Chr( 65 + 8 % 26 ) + Str( 8, 3 ), 40 )
   a[ 4 ] += 3 - n % 7
   IF n % 3 == 25 % 3
      n += 25
   ELSE
      c := Upper( Left( c, 25 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 64 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 87
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 83, n )
   c := iif( "C" $ c, StrTran( c, "C", "-" ), c + "C" )
   d := Round( d, 2 ) + Int( n / 11 )
   n += iif( d > n, 1, -1 ) * 33
   ? 1067, n, Str( d, 18, 4 ), c, a[ 1 ], Len( h )
   n := ( n * 31 + 86 ) % 100003
   d := d * 0.75 - 55 / 8
   c := Right( c + Chr( 65 + 48 % 26 ) + Str( 48, 3 ), 40 )
   a[ 1 ] += 30 - n % 7
   IF n % 3 == 64 % 3
      n += 64
   ELSE
      c := Upper( Left( c, 64 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 5 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 90
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 44, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 54 )
   n += iif( d > n, 1, -1 ) * 47
   ? 1079, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 51 ) % 100003
   d := d * 0.75 - 26 / 8
   c := Right( c + Chr( 65 + 1 % 26 ) + Str( 1, 3 ), 40 )
   a[ 9 ] += 38 - n % 7
   IF n % 3 == 95 % 3
      n += 95
   ELSE
      c := Upper( Left( c, 95 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 65 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 9
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 27, n )
   c := iif( "M" $ c, StrTran( c, "M", "-" ), c + "M" )
   d := Round( d, 2 ) + Int( n / 26 )
   n += iif( d > n, 1, -1 ) * 40
   ? 1091, n, Str( d, 18, 4 ), c, a[ 6 ], Len( h )
   n := ( n * 31 + 30 ) % 100003
   d := d * 0.75 - 60 / 8
   c := Right( c + Chr( 65 + 29 % 26 ) + Str( 29, 3 ), 40 )
   a[ 5 ] += 34 - n % 7
   IF n % 3 == 38 % 3
      n += 38
   ELSE
      c := Upper( Left( c, 38 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 14 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k2" ] := n
   CASE n % 4 == 1
      h[ "k2" ] := Len( c )
   OTHERWISE
      n -= 80
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 64, n )
   c := iif( "B" $ c, StrTran( c, "B", "-" ), c + "B" )
   d := Round( d, 2 ) + Int( n / 24 )
   n += iif( d > n, 1, -1 ) * 29
   ? 1103, n, Str( d, 18, 4 ), c, a[ 4 ], Len( h )
   n := ( n * 31 + 54 ) % 100003
   d := d * 0.75 - 86 / 8
   c := Right( c + Chr( 65 + 8 % 26 ) + Str( 8, 3 ), 40 )
   a[ 8 ] += 77 - n % 7
   IF n % 3 == 19 % 3
      n += 19
   ELSE
      c := Upper( Left( c, 19 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 51 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k7" ] := n
   CASE n % 4 == 1
      h[ "k7" ] := Len( c )
   OTHERWISE
      n -= 7
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 28, n )
   c := iif( "E" $ c, StrTran( c, "E", "-" ), c + "E" )
   d := Round( d, 2 ) + Int( n / 77 )
   n += iif( d > n, 1, -1 ) * 19
   ? 1115, n, Str( d, 18, 4 ), c, a[ 5 ], Len( h )
   n := ( n * 31 + 7 ) % 100003
   d := d * 0.75 - 91 / 8
   c := Right( c + Chr( 65 + 8 % 26 ) + Str( 8, 3 ), 40 )
   a[ 5 ] += 24 - n % 7
   IF n % 3 == 51 % 3
      n += 51
   ELSE
      c := Upper( Left( c, 51 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 58 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k1" ] := n
   CASE n % 4 == 1
      h[ "k1" ] := Len( c )
   OTHERWISE
      n -= 92
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 41, n )
   c := iif( "Q" $ c, StrTran( c, "Q", "-" ), c + "Q" )
   d := Round( d, 2 ) + Int( n / 15 )
   n += iif( d > n, 1, -1 ) * 11
   ? 1127, n, Str( d, 18, 4 ), c, a[ 3 ], Len( h )
   n := ( n * 31 + 43 ) % 100003
   d := d * 0.75 - 25 / 8
   c := Right( c + Chr( 65 + 24 % 26 ) + Str( 24, 3 ), 40 )
   a[ 5 ] += 84 - n % 7
   IF n % 3 == 68 % 3
      n += 68
   ELSE
      c := Upper( Left( c, 68 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 96 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 60
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 5, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 86 )
   n += iif( d > n, 1, -1 ) * 93
   ? 1139, n, Str( d, 18, 4 ), c, a[ 10 ], Len( h )
   n := ( n * 31 + 48 ) % 100003
   d := d * 0.75 - 43 / 8
   c := Right( c + Chr( 65 + 57 % 26 ) + Str( 57, 3 ), 40 )
   a[ 3 ] += 22 - n % 7
   IF n % 3 == 14 % 3
      n += 14
   ELSE
      c := Upper( Left( c, 14 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 1 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k11" ] := n
   CASE n % 4 == 1
      h[ "k11" ] := Len( c )
   OTHERWISE
      n -= 11
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 36, n )
   c := iif( "L" $ c, StrTran( c, "L", "-" ), c + "L" )
   d := Round( d, 2 ) + Int( n / 45 )
   n += iif( d > n, 1, -1 ) * 54
   ? 1151, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 72 ) % 100003
   d := d * 0.75 - 27 / 8
   c := Right( c + Chr( 65 + 49 % 26 ) + Str( 49, 3 ), 40 )
   a[ 7 ] += 46 - n % 7
   IF n % 3 == 40 % 3
      n += 40
   ELSE
      c := Upper( Left( c, 40 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 56 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k12" ] := n
   CASE n % 4 == 1
      h[ "k12" ] := Len( c )
   OTHERWISE
      n -= 12
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 7, n )
   c := iif( "N" $ c, StrTran( c, "N", "-" ), c + "N" )
   d := Round( d, 2 ) + Int( n / 61 )
   n += iif( d > n, 1, -1 ) * 26
   ? 1163, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 70 ) % 100003
   d := d * 0.75 - 58 / 8
   c := Right( c + Chr( 65 + 25 % 26 ) + Str( 25, 3 ), 40 )
   a[ 3 ] += 42 - n % 7
   IF n % 3 == 47 % 3
      n += 47
   ELSE
      c := Upper( Left( c, 47 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 95 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k9" ] := n
   CASE n % 4 == 1
      h[ "k9" ] := Len( c )
   OTHERWISE
      n -= 61
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 4, n )
   c := iif( "D" $ c, StrTran( c, "D", "-" ), c + "D" )
   d := Round( d, 2 ) + Int( n / 53 )
   n += iif( d > n, 1, -1 ) * 32
   ? 1175, n, Str( d, 18, 4 ), c, a[ 2 ], Len( h )
   n := ( n * 31 + 52 ) % 100003
   d := d * 0.75 - 6 / 8
   c := Right( c + Chr( 65 + 49 % 26 ) + Str( 49, 3 ), 40 )
   a[ 6 ] += 5 - n % 7
   IF n % 3 == 60 % 3
      n += 60
   ELSE
      c := Upper( Left( c, 60 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 9 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 8
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 33, n )
   c := iif( "Z" $ c, StrTran( c, "Z", "-" ), c + "Z" )
   d := Round( d, 2 ) + Int( n / 96 )
   n += iif( d > n, 1, -1 ) * 9
   ? 1187, n, Str( d, 18, 4 ), c, a[ 9 ], Len( h )
   n := ( n * 31 + 44 ) % 100003
   d := d * 0.75 - 47 / 8
   c := Right( c + Chr( 65 + 35 % 26 ) + Str( 35, 3 ), 40 )
   a[ 4 ] += 43 - n % 7
   IF n % 3 == 79 % 3
      n += 79
   ELSE
      c := Upper( Left( c, 79 % 7 ) ) + Lower( SubStr( c, 8 ) )
   ENDIF
   FOR i := 1 TO 6 % 5 + 1
      d += i * 0.25
   NEXT
   DO CASE
   CASE n % 4 == 0
      h[ "k8" ] := n
   CASE n % 4 == 1
      h[ "k8" ] := Len( c )
   OTHERWISE
      n -= 34
   ENDCASE
   n := Eval( b, n % 1000 ) + Mix( 96, n )
   c := iif( "O" $ c, StrTran( c, "O", "-" ), c + "O" )
   d := Round( d, 2 ) + Int( n / 89 )
   n += iif( d > n, 1, -1 ) * 41
   ? 1199, n, Str( d, 18, 4 ), c, a[ 7 ], Len( h )
   n := ( n * 31 + 39 ) % 100003
   ? n, d, c
   AEval( a, {| x | QQOut( "", x ) } )
   ?
RETURN

STATIC FUNCTION Mix( x, y )
RETURN ( x * 7 + y ) % 13
