// The CA-Tools math names are not reserved: a program's own function of one of them comes first, in compiled code and
// in a macro (a STATIC one in the module's own code only: a macro reaches the runtime's), the others stay the
// runtime's.
PROCEDURE Main()
   ? Sign( -5 ), Pi(), &( "Sign( 3 )" ), &( "Pi()" ), Floor( 2.5 ), Cos( 0 )
   RETURN

FUNCTION Sign( n )
   RETURN IIf( n < 0, "negative", "not negative" )

STATIC FUNCTION Pi()
   RETURN 3
