// Doubles that hold integers (literals, powers, remainders and quotients) as text: Str() with no width, widths and
// decimals, StrZero(), Transform() and the default display, near the limits of 2^31, 10^9, 2^52, 2^53 and 10^15.
PROCEDURE Main()
   LOCAL aV := { 0.0, -0.0, 1.0, -1.0, 7.0, 10.0, -10.0, 99.0, 100.0, 12345.0, -98765.0, 2 ^ 31, -(2 ^ 31), ;
                 999999999.0, 1000000000.0, 2 ^ 52 - 1, -(2 ^ 52 - 1), 2 ^ 52, 2 ^ 53, 10 ^ 15, -(10 ^ 15), 123456789012345.0, ;
                 4 % 2, 7 % 3, -7 % 3, 10 / 2, 1000 / 10 }, v, w, d, c := 0
   FOR EACH v IN aV
      ?? "[" + Str( v ) + "]", "[" + Str( v, 5 ) + "]", "[" + StrZero( v, 8 ) + "]"
      FOR w := 1 TO 22 STEP 3
         FOR d := 0 TO 4
            ?? "[" + Str( v, w, d ) + "]"
         NEXT
      NEXT
      ?? "[" + Transform( v, "999,999.99" ) + "]", "[" + Transform( v, "@Z 9999" ) + "]", v, -v, v * 1
      ?
   NEXT
   RETURN
