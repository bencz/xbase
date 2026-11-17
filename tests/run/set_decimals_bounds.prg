// SET DECIMALS at its bounds: 0 and 65535 (the most a number keeps: Str() of a quotient then has as many decimals)
// are taken; 65536, 65538, 70000, a value past 32 bits and a negative one are argument error 2020, the setting
// unchanged (the reference keeps 65538 and formats with 2 decimals).
PROCEDURE Main()
   LOCAL n, e
   FOR EACH n IN { 0, 65535, 65536, 65538, 70000, 2 ^ 40, -1, 2 }
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         Set( _SET_DECIMALS, n )
         ?? "[" + LTrim( Str( n ) ) + " taken:", Set( _SET_DECIMALS ), Len( Str( 1 / 3 ) ), "]"
      RECOVER USING e
         ?? "[" + LTrim( Str( n ) ) + " refused:", e:subCode, e:operation, Set( _SET_DECIMALS ), "]"
      END SEQUENCE
      ?
   NEXT
RETURN
