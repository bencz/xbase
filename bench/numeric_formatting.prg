// Number formatting: 80,000 numbers with three decimals (-50,000 to 50,000) and integers (0 to 999,999) from a fixed
// seed, each formatted with Str (width and decimals), StrZero, Round, and Transform with numeric pictures (thousands
// separators, @E, @( for negatives in parentheses, @Z) and @R on a string (a CPF mask), and read back with Val. The
// text's length, its separators and the values read back are checked. No number lies halfway between two values of
// two decimals: the reference rounds some of those (as 8904.185) up where xbase's Str and Transform round down.
STATIC nSeed := 314159

PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, x, n, c := "", cLine, nCents := 0, nTenths := 0, nComma, nDot, nParen
   FOR i := 1 TO 80000
      x := Rand( 100000000 ) - 50000000
      IF Abs( x ) % 10 == 5
         x++
      ENDIF
      x /= 1000
      n := Int( Rand( 1000000 ) )
      cLine := Str( x, 14, 2 ) + StrZero( n, 8 ) + Transform( x, "@E 999,999.99" ) + ;
               Transform( x, "999,999.999" ) + Transform( StrZero( n, 11 ), "@R 999.999.999-99" ) + ;
               Transform( x, "@( 99,999.99" ) + Transform( n % 3, "@Z 9" ) + LTrim( Str( Round( x, 1 ), 12, 1 ) ) + ;
               Chr( 10 )
      c += cLine
      nCents += Round( Val( Str( x, 14, 2 ) ) * 100, 0 )
      nTenths += Round( Round( x, 1 ) * 10, 0 )
   NEXT
   nComma := Len( c ) - Len( StrTran( c, "," ) )
   nDot := Len( c ) - Len( StrTran( c, "." ) )
   nParen := Len( c ) - Len( StrTran( c, "(" ) )
   // The first line, the counts and the text as a whole were checked against the reference implementation; the sums
   // of cents and tenths against a C computation.
   ? "numeric_formatting", ElapsedMs() - t, ;
     IIf( Len( c ) == 6065015 .AND. nComma == 236856 .AND. nDot == 558428 .AND. nParen == 41318 .AND. ;
          nCents == -6496080269 .AND. nTenths == -649608444 .AND. Left( c, 75 ) == ;
          "      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0" + Chr( 10 ), "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
