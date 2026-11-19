// The CA-Tools math functions: each result is the correctly rounded double (compared with the value computed to 80
// digits), whatever the argument's size (Sin( 10^22 ) reduces it exactly, ATan() and Atn2() of numbers near the
// largest double and Cot() of ones near the least), with the sign of a zero kept by Atn2(); the display, the domain's
// limits (stars for a NaN or an infinity), Floor(), Ceiling(), Sign(), Fact() and Pi(); an argument error with
// xbase's codes (the reference gives 0), in compiled code and in a macro.
PROCEDURE Main()
   LOCAL e, b
   Check( "Sin( 1.0 )", Sin( 1.0 ), 0.8414709848078965 )
   Check( "Cos( 1.0 )", Cos( 1.0 ), 0.5403023058681398 )
   Check( "Tan( 1.0 )", Tan( 1.0 ), 1.5574077246549023 )
   Check( "Sin( 10000000000000000000000.0 )", Sin( 10000000000000000000000.0 ), -0.8522008497671888 )
   Check( "Cos( 10000000000000000000000.0 )", Cos( 10000000000000000000000.0 ), 0.523214785395139 )
   Check( "Sin( 0.5 )", Sin( 0.5 ), 0.479425538604203 )
   Check( "Cos( 100.0 )", Cos( 100.0 ), 0.8623188722876839 )
   Check( "Tan( 1.5707963267948966 )", Tan( 1.5707963267948966 ), 16331239353195370 )
   Check( "Sin( 3.141592653589793 )", Sin( 3.141592653589793 ), 0.00000000000000012246467991473532 )
   Check( "Cos( 1.5707963267948966 )", Cos( 1.5707963267948966 ), 0.00000000000000006123233995736766 )
   Check( "Sin( -2.5 )", Sin( -2.5 ), -0.5984721441039565 )
   Check( "ATan( 1.0 )", ATan( 1.0 ), 0.7853981633974483 )
   Check( "ATan( -0.3 )", ATan( -0.3 ), -0.2914567944778671 )
   Check( "ATan( 10000000000.0 )", ATan( 10000000000.0 ), 1.5707963266948965 )
   Check( "ASin( 0.5 )", ASin( 0.5 ), 0.5235987755982989 )
   Check( "ACos( 0.5 )", ACos( 0.5 ), 1.0471975511965979 )
   Check( "ASin( -0.9999 )", ASin( -0.9999 ), -1.5566540733173846 )
   Check( "ACos( -0.25 )", ACos( -0.25 ), 1.8234765819369754 )
   Check( "Sinh( 1.0 )", Sinh( 1.0 ), 1.1752011936438014 )
   Check( "Cosh( 1.0 )", Cosh( 1.0 ), 1.5430806348152437 )
   Check( "Tanh( 0.5 )", Tanh( 0.5 ), 0.46211715726000974 )
   Check( "Sinh( -0.01 )", Sinh( -0.01 ), -0.010000166667500003 )
   Check( "Cosh( 30.0 )", Cosh( 30.0 ), 5343237290762.231 )
   Check( "Tanh( 20.0 )", Tanh( 20.0 ), 1.0 )
   Check( "Log10( 2.0 )", Log10( 2.0 ), 0.3010299956639812 )
   Check( "Log10( 1000.0 )", Log10( 1000.0 ), 3.0 )
   Check( "Log10( 0.001 )", Log10( 0.001 ), -3.0 )
   Check( "DToR( 180.0 )", DToR( 180.0 ), 3.141592653589793 )
   Check( "DToR( 45.0 )", DToR( 45.0 ), 0.7853981633974483 )
   Check( "RToD( 1.0 )", RToD( 1.0 ), 57.29577951308232 )
   Check( "RToD( 3.141592653589793 )", RToD( 3.141592653589793 ), 180.0 )
   Check( "Atn2( 1.0, 1.0 )", Atn2( 1.0, 1.0 ), 0.7853981633974483 )
   Check( "Atn2( -1.0, -1.0 )", Atn2( -1.0, -1.0 ), -2.356194490192345 )
   Check( "Atn2( 1.0, 0.0 )", Atn2( 1.0, 0.0 ), 1.5707963267948966 )
   Check( "Atn2( 0.0, -1.0 )", Atn2( 0.0, -1.0 ), 3.141592653589793 )
   Check( "Atn2( 3.0, -4.0 )", Atn2( 3.0, -4.0 ), 2.498091544796509 )
   Check( "Cot( 1.0 )", Cot( 1.0 ), 0.6420926159343308 )
   Check( "ASin( 0.4522411525616692 )", ASin( 0.4522411525616692 ), 0.46927653959543897 )
   Check( "ASin( 0.4955248761471751 )", ASin( 0.4955248761471751 ), 0.5184390102139447 )
   Check( "ACos( 0.8802805410354104 )", ACos( 0.8802805410354104 ), 0.4943431573482534 )
   Check( "ACos( 0.868656300218908 )", ACos( 0.868656300218908 ), 0.518312759718241 )
   Check( "ATan( 10 ^ 305 )", ATan( 10 ^ 305 ), 1.5707963267948966 )
   Check( "Atn2( 10 ^ 305, 3 * 10 ^ 304 )", Atn2( 10 ^ 305, 3 * 10 ^ 304 ), 1.2793395323170296 )
   Check( "Atn2( 10 ^ 305, 0 )", Atn2( 10 ^ 305, 0 ), 1.5707963267948966 )
   ? Sin( 1 ), Cos( 1 ), Tan( 1 ), ATan( 1 ), Pi(), Log10( 2 ), DToR( 90 ), RToD( 1 ), Len( Str( Sin( 1 ) ) )
   ? ASin( 2 ), ACos( -2 ), Log10( 0 ), Log10( -1 ), Cot( 0 ), Sinh( 1000 ), Cosh( -1000 ), Tanh( 1000 )
   ? Cot( 10 ^ -305 ) == 10 ^ 305, Cot( -( 10 ^ -320 ) ), Atn2( Atn2( -0.0, 1 ), -1 ), Atn2( Atn2( 0.0, 1 ), -1 )
   ? Floor( 2.7 ), Floor( -2.7 ), Ceiling( 2.2 ), Ceiling( -2.2 ), Floor( 5 ), Ceiling( -0.5 ), Floor( 2 ^ 70 )
   ? Sign( -3 ), Sign( 0 ), Sign( 2.5 ), Sign( -0.0001 ), ValType( Floor( 2.7 ) ), Len( Str( Floor( 2.7 ) ) )
   ? Fact( 0 ), Fact( 5 ), Fact( 5.7 ), Fact( 20 ), Fact( -1 ), Fact( 171 ), Fact( Log( -1 ) - Log( -1 ) )
   ? Fact( 21 ) == 51090942171709440000, Fact( 25 ) == 15511210043330985984000000, Str( Fact( 170 ) / 10 ^ 306, 20, 12 )
   SET DECIMALS TO 6
   ? Sin( 1 ), Pi(), Len( Str( Pi() ) )
   SET DECIMALS TO 2
   b := ErrorBlock( {| x | Break( x ) } )
   FOR EACH e IN { "Sin", "Cos", "Tan", "Cot", "ASin", "ACos", "ATan", "Sinh", "Cosh", "Tanh", "Log10", "DToR", ;
                   "RToD", "Floor", "Ceiling", "Sign", "Fact" }
      ?? "", Code( e )
   NEXT
   ?? "", Code( "Atn2", "1, 'a'" )
   ErrorBlock( b )
   ? &( "Sin( 0.5 )" ), &( "Atn2( 1, 1 )" ) == 0.7853981633974483, Abs( 1.5 ), Exp( 0 )
   RETURN

STATIC FUNCTION Check( cCall, nGot, nWant )
   IF nGot != nWant
      ? cCall, "gives", Str( nGot, 30, 20 ), "instead of", Str( nWant, 30, 20 )
   ENDIF
   RETURN NIL

STATIC FUNCTION Code( cFunction, cArgs )
   LOCAL e
   BEGIN SEQUENCE
      &( cFunction + "( " + IIf( cArgs == NIL, "'a'", cArgs ) + " )" )
   RECOVER USING e
      RETURN e:subCode
   END SEQUENCE
   RETURN 0
