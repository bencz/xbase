// A hexadecimal literal of up to 64 bits is the integer those bits make in two's complement, compiled or in a macro;
// leading zeros do not count. Its width is a decimal literal's of the same value: from 10^9, a column per digit and
// one more; 20 for -10^9 and below.
PROCEDURE Main()
   LOCAL a := { "0xFFFFFFFFFFFFFFFF", "0x8000000000000000", "0x8000000000000001", "0x7FFFFFFFFFFFFFFF", ;
                "0xFFFFFFFF", "0x80000000", "0x00000000000000000001", "0x0", "-0xFFFFFFFFFFFFFFFF" }, c
   ? 0xFFFFFFFFFFFFFFFF, 0x8000000000000000, 0x8000000000000001, 0x7FFFFFFFFFFFFFFF
   ? 0xFFFFFFFF, 0x80000000, 0x00000000000000000001, 0x0, -0xFFFFFFFFFFFFFFFF
   ? 0x8000000000000001 + 0, ValType( 0xFFFFFFFFFFFFFFFF ), Len( Str( 0xFFFFFFFFFFFFFFFF ) ), ;
     0xFFFFFFFFFFFFFFFF == -1, 0xFFFFFFFFFFFFFF00 & 0xFF0, 0xFFFFFFFFFFFFFFFF >> 60
   FOR EACH c IN a
      ? c, &c, Len( Str( &c ) )
   NEXT
   a := { 0x3B9AC9FF, 0x3B9ACA00, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000, 0x2540BE3FF, 0x2540BE400, ;
          0xFFFFFFFFFF, 0x38D7EA4C67FFF, 0x2386F26FC0FFFF, 0x2386F26FC10000, 0xDE0B6B3A763FFFF, 0xDE0B6B3A7640000, ;
          0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF80000000, 0xFFFFFFFF7FFFFFFF, 0xFFFFFFFFC4653600, ;
          0xFFFFFFFFC46535FF, 0xFFFFFFFFFFFFFFF0 }
   FOR EACH c IN a
      ? c, Len( Str( c ) ), &( Hex( c ) ), Len( Str( &( Hex( c ) ) ) )
   NEXT
   ? Type( "0x1FFFFFFFFFFFFFFFF" )
   RETURN

STATIC FUNCTION Hex( n )
   LOCAL s := "", k
   FOR k := 60 TO 0 STEP -4
      s += SubStr( "0123456789ABCDEF", ( ( n >> k ) & 0xF ) + 1, 1 )
   NEXT
   RETURN "0x" + s
