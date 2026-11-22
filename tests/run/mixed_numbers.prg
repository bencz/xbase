// Integers and doubles met in the same operation inside loops: elements read from arrays that hold both, so the
// path of an integer with a double runs now and then among iterations of two integers or two doubles.
PROCEDURE Main()
   LOCAL a := { 1, 2.5, 3, 4.25, 5, 6, 7.5, 8 }, b := { 2, 2, 0.5, 4, 1.5, 3, 2, 0.25 }
   LOCAL i, n, s, p, c, lt, d

   s := 0
   p := 0
   c := 0
   FOR n := 1 TO 1000
      FOR i := 1 TO Len( a )
         s := s + ( a[ i ] + b[ i ] )
         p := p + ( a[ i ] * b[ i ] )
         d := a[ i ] - b[ i ]
         IF a[ i ] < b[ i ]
            c++
         ENDIF
      NEXT
   NEXT
   ? s, p, c, d

   // Each operation alone: two integers, two doubles, an integer with a double, a double with an integer.
   FOR i := 1 TO Len( a ) - 1
      ? a[ i ] + a[ i + 1 ], a[ i ] - b[ i + 1 ], a[ i ] * b[ i ], a[ i ] > b[ i + 1 ], a[ i ] == b[ i ]
   NEXT

   // An array changing from integers to doubles while a loop runs over it.
   lt := Array( 6 )
   AFill( lt, 1 )
   FOR i := 2 TO Len( lt )
      lt[ i ] := lt[ i - 1 ] * IIf( i % 2 == 0, 1.5, 2 )
   NEXT
   FOR i := 1 TO Len( lt )
      ?? lt[ i ], ""
   NEXT
   ?
   RETURN
