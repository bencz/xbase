// ^, Exp() and Log() give the correctly rounded double on every system (xbase's own double-double arithmetic, not the
// C library's, whose last bit differs between systems; the reference uses the C library's): arguments where common
// libraries round wrongly, compared with the exact double written as a literal, as constants (folded when compiled),
// as values known only at run time (Val(), array elements) and through a macro; a power exactly halfway between two
// doubles (7^19), which rounds to the even one, and one just off it (3^35); n ^ -0.5 of integers whose square root is
// no double; subnormal results of Exp() and ^ (the general path and the exact one, positive and negative powers),
// rounded once at the subnormal quantum (scaled back by 2^1074 to an integer); powers of a negative base, of zero,
// too large, and with a fractional exponent of a perfect square; Exp() past the doubles (0, as Clipper) and Log() of 0
// and of a negative number.
PROCEDURE Main()
   LOCAL a := { 6.218, 2.467, -29.375, 25.3, -0.47, 1.53, -0.5, 33, 0.38, 16.07, 3.73, 22, 1075, 2.17 }, c
   LOCAL k := 2 ^ 537
   ? "Exp constant:", Exp( 6.218 ) == 501.6988302927266, Exp( 2.467 ) == 11.78703265894112, ;
     Exp( -29.375 ) == 0.0000000000001748234128271057
   ? "Exp run time:", Exp( a[ 1 ] ) == 501.6988302927266, Exp( a[ 2 ] ) == 11.78703265894112, ;
     Exp( Val( "-29.375" ) ) == 0.0000000000001748234128271057
   ? "pow constant:", 25.3 ^ -0.47 == 0.21904500543819952, 1.53 ^ 2.17 == 2.516404220292426, ;
     33 ^ 0.38 == 3.776028792389227, 16.07 ^ 3.73 == 31509.333750592505
   ? "pow run time:", a[ 4 ] ^ a[ 5 ] == 0.21904500543819952, a[ 6 ] ^ a[ 14 ] == 2.516404220292426, ;
     a[ 8 ] ^ a[ 9 ] == 3.776028792389227, a[ 10 ] ^ a[ 11 ] == 31509.333750592505
   c := "16.07 ^ 3.73"
   ? "pow macro:", &c == 31509.333750592505, Log( Val( "31509.333750592505" ) ) == Log( 31509.333750592505 )
   ? "halfway:", 7 ^ 19 == 11398895185373144, 3 ^ 35 == 50031545098999704, Val( "7" ) ^ 19 == 11398895185373144
   ? "roots:", 22 ^ -0.5 == 0.21320071635561044, a[ 12 ] ^ a[ 7 ] == 0.21320071635561044, ;
     1075 ^ -0.5 == 0.030499714066520932, a[ 13 ] ^ -0.5 == 0.030499714066520932
   ? "subnormal:", Exp( -709.1115996822651 ) * k * k == 2202726352373771, ;
     ( 2 ^ 1023 ) ^ -1.0000000000000002 * k * k == 2251799813684893, ;
     ( 27 * 2 ^ -90 ) ^ 12 * k * k == 2345228676515611, ( 13 * 2 ^ 99 ) ^ -10 * k * k == 140309188461315
   ? "exact:", 2 ^ 10, 10 ^ -2 == 0.01, ( -2 ) ^ 3, ( -2 ) ^ 2, 0 ^ 0, 0.25 ^ 0.5 == 0.5, 1046529 ^ 1.5 == 1070599167
   ? "edges:", Exp( 1000 ), Str( Exp( -1000 ) ), Log( 1 ), Str( Log( 0 ) ), Str( Log( -1 ) ), Str( 10 ^ 400 ), ;
     2 ^ -1074 > 0
   ? "Log:", Log( 10 ) == 2.302585092994046, Log( Val( "0.000001" ) ) == -13.815510557964274
RETURN
