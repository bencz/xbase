// Arithmetic and comparisons of two doubles whose types are known only at run time (array elements), done in
// place: the decimals each operator gives (the larger of the two for + and -, their sum for *, SET DECIMALS for / and
// %), comparisons, a zero divisor, and an integer with a double.
PROCEDURE Main()
   LOCAL v := { 1.5, 0.25, 3.125, -2.0, 7.5, 0.0, 10 }, e
   ? "add", v[ 1 ] + v[ 2 ], v[ 3 ] + v[ 4 ], v[ 1 ] + v[ 7 ]
   ? "sub", v[ 1 ] - v[ 2 ], v[ 4 ] - v[ 3 ], v[ 7 ] - v[ 2 ]
   ? "mul", v[ 1 ] * v[ 2 ], v[ 3 ] * v[ 4 ], v[ 2 ] * v[ 3 ] * v[ 3 ], v[ 7 ] * v[ 1 ]
   ? "div", v[ 5 ] / v[ 1 ], v[ 3 ] / v[ 4 ], v[ 7 ] / v[ 2 ]
   ? "mod", v[ 5 ] % v[ 1 ], v[ 3 ] % v[ 2 ], v[ 4 ] % v[ 1 ], v[ 7 ] % v[ 5 ]
   ? "cmp", v[ 1 ] < v[ 2 ], v[ 1 ] > v[ 2 ], v[ 1 ] == v[ 1 ], v[ 4 ] <= v[ 6 ], v[ 7 ] >= v[ 5 ]
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
