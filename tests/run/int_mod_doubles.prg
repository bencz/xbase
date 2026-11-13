// Int() of doubles (with a width of their own or the default one, large and small, negative, past 64 bits, NaN) and
// % of doubles that hold integers beyond 2^53 (a linear congruential generator's state, as xBase programs keep it):
// the values and display widths are the runtime's. Int( 2.0 ^ 62 ) is that integer exactly (the reference shows a
// rounded one).
PROCEDURE Main()
   LOCAL x := 7.0, i, s := 0, d
   FOR i := 1 TO 5
      x := ( x * 1103515245 + 12345 ) % 2147483648
      s += Int( x / 65536 ) % 100
   NEXT
   ? x, s
   ? ( 2.0 ^ 60 + 5 ) % 7, ( -( 2.0 ^ 61 ) - 3 ) % 1000, ( 2.0 ^ 53 + 2 ) % 10, 1000000000000000000.0 % 7, -1000000000000000000.0 % -7
   FOR EACH d IN { 1 / 3, 12345.678, -0.5, -12345.678, 9999999999.5, 10000000000.5, -999999999.5, -1000000000.5, ;
         10000000000000000000.0, -10000000000000000000.0, 2.0 ^ 62, Val( "123.45" ), 7 / 2 }
      ? Int( d ), Len( Str( Int( d ) ) ), ValType( Int( d ) )
   NEXT
   ? Int( Log( -1 ) ), Int( 10.0 ^ 300 ) == 10.0 ^ 300
RETURN
