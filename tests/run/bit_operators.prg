// xbase's bit operators & | ^^ << >> ~ and &= |= ^^= <<= >>=: on constants (folded when compiled) and on
// variables (machine operations in place), which must agree, and agree with the bit functions; precedence (C's:
// the shifts between + - and the comparisons, & above ^^ above |, all three below the comparisons); >> keeps the
// sign; counts outside 0..63; doubles taken as integers; codeblocks, whose | still delimits the parameters; the
// argument errors, which a handler may substitute.
// Rules whose markers are written right next to < > = operators: the operators keep their meaning (the program
// compiles only if the tokens are right).
#xtranslate LT( <a>, <b> ) => ( <a><<b> )
#xtranslate GE( <a>, <b> ) => <a>>=<b>
#xtranslate NE( <a>, <b> ) => <a><><b>
#xtranslate SH( <n>, <c> ) => ( <n>>><c> )
#xtranslate SL( <n>, <c> ) => ( <n><<<c> )
#xtranslate HALFPLUS( <n> ) => ( <n> + ( k>>1 ) )
#xtranslate FIRSTGT( <x,...> ) => ( <x,...>>1 )
#xcommand SHIFT <v> BY <c> => <v>>>=<c>

PROCEDURE Main()
   LOCAL a := 0xF0, b := 0x3C, n := -8, k, x, d := 1000 / 4, s := "s", e, nil, big := 0x7FFFFFFFFFFFFFFF

   ? "ops", 0xF0 & 0x3C, a & b, 0xF0 | 0x3C, a | b, 0xF0 ^^ 0x3C, a ^^ b, ~0xF0, ~a, ~-1, ~0
   ? "shl", 0xF0 << 4, a << 4, 1 << 63, 1 << 64, 1 << -1, n << 1
   k := 63
   ? "shl var", 1 << k, ( 1 << 63 ) == ( 1 << k )
   k := 64
   ?? "", 1 << k
   ? "shr", 0xF0 >> 4, a >> 4, -8 >> 1, n >> 1, -1 >> 63, n >> 64, 8 >> 64, -8 >> -1, big >> 62, -big >> 62
   ? "vs functions", BitAnd( a, b ) == ( a & b ), BitOr( a, b ) == ( a | b ), BitXor( a, b ) == ( a ^^ b ), ;
     BitNot( a ) == ~a, BitShl( a, 4 ) == ( a << 4 ), BitShr( n, 1 ) == ( n >> 1 ), BitShr( -8, 1 ), -8 >> 1

   k := 6
   x := 64
   SHIFT x BY 2
   ? "rules", LT( 1, 2 ), GE( 5, 6 ), NE( 7, 8 ), SH( 64, 2 ), SL( 1, 3 ), HALFPLUS( 10 ), FIRSTGT( 4 ), x

   // Precedence and associativity.
   ? "prec", 1 + 2 << 3, 1 << 2 + 3, 2 * 3 << 1, 1 | 2 ^^ 3 & 4, 6 & 5 | 8, 6 | 5 & 8, 1 ^^ 3 | 4, 5 > 2 << 1
   ? "prec2", ( 6 & 3 ) == 2, -1 >> 1 >> 62, 1 << 2 << 3, 100 - 1 >> 2, ~1 + 1, - ~5, ~~7, 2 ^ 3 << 1
   // A comparison binds tighter than & ^^ |, as in C: ( x & m ) == 0 needs its parentheses (see the errors).
   ? "logic", .NOT. ( 1 & 0 ) == 0, ( 1 & 1 ) == 1 .AND. ( 2 | 1 ) == 3, ( 4 & 4 ) != 0

   // Doubles are taken as integers: truncated toward zero, saturated, NaN 0; the result is always an integer.
   ? "dbl", d & 0xFF, 7.9 & 3, -7.9 & 0xFF, 1.5 << 2, ( 2 ^ 100 ) >> 62, -( 2 ^ 100 ) >> 62, 2.9 ^^ 1, ~2.5
   ?? "", ValType( d & 1 )
   x := 10.75
   ? "dbl var", x & 0xFF, x | 0, x << 1, x >> 1, ~x, ValType( x >> 1 ), Len( Str( x & 1 ) )

   // Compound assignments, also on elements.
   x := 5
   x &= 3
   ?? " &=", x
   x |= 0x10
   ?? " |=", x
   x ^^= 0xFF
   ?? " ^^=", x
   x <<= 2
   ?? " <<=", x
   x >>= 3
   ?? " >>=", x
   k := { 0xFF, { 1 } }
   k[ 1 ] &= 0x0F
   k[ 2 ][ 1 ] <<= 10
   k[ 2, 1 ] |= 3
   ? "elements", k[ 1 ], k[ 2 ][ 1 ]

   // Codeblocks: | delimits the parameters and is the operator in the body.
   ? "blocks", Eval( {| p | p & 1 }, 3 ), Eval( {|| 2 | 1 } ), Eval( {| p, q | p | q }, 4, 1 ), { 1 | 2, 3 & 1 }[ 1 ], ;
     Eval( {| p | ~p >> 1 }, 0 )

   // Typical uses: CRC-32 bit by bit, a byte swap, a rotation.
   ? "crc", Crc32( "123456789" ), Crc32( "" ), Hex( Swap32( 0x11223344 ) ), Hex( Rotl32( 0x80000001, 4 ) )

   // Errors: the operator's argument error, with its operation's code, which a handler may substitute.
   ErrorBlock( {| err | Break( err ) } )
   FOR EACH k IN { {|| s & 1 }, {|| 1 | s }, {|| s ^^ 1 }, {|| s << 1 }, {|| 1 >> s }, {|| ~s }, {|| nil & 1 }, ;
                   {|| .T. | 1 }, {|| 6 & 3 == 2 } }
      BEGIN SEQUENCE
         Eval( k )
      RECOVER USING e
         ? "error", e:genCode, e:subCode, e:description, e:operation, Len( e:args ), e:canSubstitute
      END
   NEXT
   ErrorBlock( {| err | 42 } )
   x := s & 1
   ? "substituted", x, ~s, s >> 1
RETURN

STATIC FUNCTION Crc32( s )
   LOCAL crc := 0xFFFFFFFF, i, j
   FOR i := 1 TO Len( s )
      crc ^^= Asc( SubStr( s, i, 1 ) )
      FOR j := 1 TO 8
         crc := ( crc >> 1 ) ^^ ( 0xEDB88320 & -( crc & 1 ) )
      NEXT
   NEXT
RETURN crc ^^ 0xFFFFFFFF

STATIC FUNCTION Swap32( n )
RETURN ( n >> 24 & 0xFF ) | ( n >> 8 & 0xFF00 ) | ( n << 8 & 0xFF0000 ) | ( n << 24 & 0xFF000000 )

STATIC FUNCTION Rotl32( n, c )
RETURN ( n << c | n >> ( 32 - c ) ) & 0xFFFFFFFF

STATIC FUNCTION Hex( n )
   LOCAL s := "", i
   FOR i := 7 TO 0 STEP -1
      s += SubStr( "0123456789ABCDEF", ( n >> ( i * 4 ) & 0xF ) + 1, 1 )
   NEXT
RETURN s
