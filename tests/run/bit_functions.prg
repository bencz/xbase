// xbase's bit functions, on constants (folded when compiled) and on variables (inline integer code), which must give
// the same values and display (also when the result is used in arithmetic): each operation, shifts at the edges of
// 0..63, negative numbers, truncated doubles.
PROCEDURE Main()
   LOCAL a := 12, b := 10, five := 5, one := 1, n1024 := 1024, minus1 := -1, k, big := 0xFFFFFFFF, d := 7.9
   ? "and", BitAnd( 12, 10 ), BitAnd( a, b )
   ? "or", BitOr( 12, 10 ), BitOr( a, b )
   ? "xor", BitXor( 12, 10 ), BitXor( a, b )
   ? "nor", BitNor( 12, 10 ), BitNor( a, b )
   ? "not", BitNot( 5 ), BitNot( five )
   ? "shl", BitShl( 1, 10 ), BitShl( one, b )
   ? "shr", BitShr( 1024, 3 ), BitShr( n1024, 3 )
   k := 63
   ? "shl63", BitShl( 1, 63 ), BitShl( one, k ) == BitShl( 1, 63 )
   k := 64
   ? "shl64", BitShl( 1, 64 ), BitShl( one, k )
   ? "shrneg", BitShr( -1, 60 ), BitShr( minus1, 60 )
   k := -1
   ? "countneg", BitShl( 1, -1 ), BitShl( one, k )
   ? "negand", BitAnd( -8, 0xFF ), BitAnd( -8 + a - 12, 255 )
   ? "xorbig", BitXor( 0xFFFFFFFF, 0x12345678 ), BitXor( big, 0x12345678 )
   ? "double", BitAnd( 7.9, 3 ), BitAnd( d, 3 )
   ? "arith", BitShl( 1, 40 ) + 0, BitShl( one, 40 ) + 0, -BitOr( 12345678901, 0 ), -BitOr( big * 3, 0 )
   ? "dblshift", BitShl( 1.0, 64 ), BitShr( -1.0, 1 ), BitShl( d, -1 ), BitShr( d, 1 )
   ? "types", ValType( BitAnd( a, b ) ), ValType( BitNot( 0 ) )
   RETURN
