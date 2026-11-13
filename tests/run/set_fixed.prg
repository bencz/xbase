// SET FIXED: numbers shown (?, QOut(), Transform() without a template or with one of only functions, padding) take
// SET DECIMALS decimals, a number of width 1 (Val( "1.5" )) its own; Str() and templates with digits do not change;
// an integer Transform() shows by its own field is two columns wider, and two per decimal. Under three SET DECIMALS.
PROCEDURE Main()
   LOCAL a := { 0, 1, -7, 1000000000, 0.5, 2.675, 1.23456, 10 / 4, 1 / 3, 123.4, 99999999999, Val( "1.5" ), ;
      Val( "12.5" ), -0.125 }, x, n
   FOR EACH n IN { 2, 0, 4 }
      SET DECIMALS TO n
      SET FIXED ON
      ? "fixed", n, Set( 2 )
      FOR EACH x IN a
         ?? "[" + Str( x ) + "]"
      NEXT
      ?
      FOR EACH x IN a
         ?? x
      NEXT
      ?
      FOR EACH x IN a
         ?? "[" + Transform( x, "" ) + "]" + "[" + Transform( x, "@Z" ) + "]" + "[" + Transform( x, "@(" ) + "]" + ;
            "[" + PadL( x, 15 ) + "]" + "[" + Transform( x, "999.99" ) + "]"
      NEXT
      ? Val( "1.5" ), Round( 2.675, 2 ), Int( 7.5 ), Len( "abc" ), 3 * 1.5
      SET FIXED OFF
      ? "nofixed", n
      FOR EACH x IN a
         ?? x
      NEXT
   NEXT
   SET FIXED ON
   ? Set( _SET_FIXED, .F. ), Set( _SET_FIXED )
RETURN
