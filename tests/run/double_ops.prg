// Arithmetic and comparisons of two doubles whose types are known only at run time (array elements), done in
// place: the decimals each operator gives (the larger of the two for + and -, their sum for *, SET DECIMALS for / and
// %), comparisons, a zero divisor, and an integer with a double. The remainder of doubles that hold integers is
// their integer remainder, fmod's: negative operands, a divisor of -1, values beyond 2^53, fractions.
PROCEDURE Main()
   LOCAL v := { 1.5, 0.25, 3.125, -2.0, 7.5, 0.0, 10 }, e, r
   ? "add", v[ 1 ] + v[ 2 ], v[ 3 ] + v[ 4 ], v[ 1 ] + v[ 7 ]
   ? "sub", v[ 1 ] - v[ 2 ], v[ 4 ] - v[ 3 ], v[ 7 ] - v[ 2 ]
   ? "mul", v[ 1 ] * v[ 2 ], v[ 3 ] * v[ 4 ], v[ 2 ] * v[ 3 ] * v[ 3 ], v[ 7 ] * v[ 1 ]
   ? "div", v[ 5 ] / v[ 1 ], v[ 3 ] / v[ 4 ], v[ 7 ] / v[ 2 ]
   ? "mod", v[ 5 ] % v[ 1 ], v[ 3 ] % v[ 2 ], v[ 4 ] % v[ 1 ], v[ 7 ] % v[ 5 ]
   ? "cmp", v[ 1 ] < v[ 2 ], v[ 1 ] > v[ 2 ], v[ 1 ] == v[ 1 ], v[ 4 ] <= v[ 6 ], v[ 7 ] >= v[ 5 ]
   r := { 17.0, 5.0, -17.0, -5.0, -1.0, 2 ^ 60, 3.0, 10, 4.0 }
   ? "rem", r[ 1 ] % r[ 2 ], r[ 3 ] % r[ 2 ], r[ 1 ] % r[ 4 ], r[ 3 ] % r[ 4 ], r[ 1 ] % r[ 5 ], r[ 3 ] % r[ 5 ]
   ? "rem", r[ 6 ] % r[ 7 ], r[ 8 ] % r[ 7 ], r[ 1 ] % r[ 8 ], r[ 8 ] % 3, r[ 9 ] % r[ 2 ] == 4, r[ 3 ] % r[ 7 ] < 0
   SET DECIMALS TO 4
   ? "decimals", v[ 5 ] / v[ 1 ], v[ 3 ] % v[ 2 ]
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      ? v[ 1 ] / v[ 6 ]
   RECOVER USING e
      ? "zero", e:subCode, e:operation
   END
   BEGIN SEQUENCE
      ? v[ 1 ] % v[ 6 ]
   RECOVER USING e
      ? "zero", e:subCode, e:operation
   END
RETURN
