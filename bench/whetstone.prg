// Whetstone (Curnow and Wichmann's synthetic benchmark, its modules scaled by a loop count of 5,000): array elements,
// an array as a parameter, conditional jumps, integer arithmetic, procedure calls with a parameter by reference,
// trigonometric functions (Sin, Cos, ATan), array references, and Sqrt( Exp( Log( x ) / t ) ). The modules the
// reference C version runs zero times (1 and 10) are left out. The check is the sum of the values each module leaves.
STATIC t := 0.499975, t1 := 0.50025, t2 := 2.0, e1, j, k, l

PROCEDURE Main()
   LOCAL tm := ElapsedMs(), nLoop := 5000, i, x, y, z, check
   e1 := { 1.0, -1.0, -1.0, -1.0 }
   FOR i := 1 TO 12 * nLoop
      e1[ 1 ] := ( e1[ 1 ] + e1[ 2 ] + e1[ 3 ] - e1[ 4 ] ) * t
      e1[ 2 ] := ( e1[ 1 ] + e1[ 2 ] - e1[ 3 ] + e1[ 4 ] ) * t
      e1[ 3 ] := ( e1[ 1 ] - e1[ 2 ] + e1[ 3 ] + e1[ 4 ] ) * t
      e1[ 4 ] := ( -e1[ 1 ] + e1[ 2 ] + e1[ 3 ] + e1[ 4 ] ) * t
   NEXT
   check := e1[ 1 ] + e1[ 2 ] + e1[ 3 ] + e1[ 4 ]
   FOR i := 1 TO 14 * nLoop
      PA( e1 )
   NEXT
   check += e1[ 1 ] + e1[ 2 ] + e1[ 3 ] + e1[ 4 ]
   j := 1
   FOR i := 1 TO 345 * nLoop
      IF j == 1
         j := 2
      ELSE
         j := 3
      ENDIF
      IF j > 2
         j := 0
      ELSE
         j := 1
      ENDIF
      IF j < 1
         j := 1
      ELSE
         j := 0
      ENDIF
   NEXT
   check += j
   j := 1
   k := 2
   l := 3
   FOR i := 1 TO 210 * nLoop
      j := j * ( k - j ) * ( l - k )
      k := l * k - ( l - j ) * k
      l := ( l - k ) * ( k + j )
      e1[ l - 1 ] := j + k + l
      e1[ k - 1 ] := j * k * l
   NEXT
   check += j + k + l + e1[ 1 ] + e1[ 2 ] + e1[ 3 ] + e1[ 4 ]
   x := 0.5
   y := 0.5
   FOR i := 1 TO 32 * nLoop
      x := t * ATan( t2 * Sin( x ) * Cos( x ) / ( Cos( x + y ) + Cos( x - y ) - 1.0 ) )
      y := t * ATan( t2 * Sin( y ) * Cos( y ) / ( Cos( x + y ) + Cos( x - y ) - 1.0 ) )
   NEXT
   check += x + y
   x := 1.0
   y := 1.0
   z := 1.0
   FOR i := 1 TO 899 * nLoop
      P3( x, y, @z )
   NEXT
   check += x + y + z
   j := 1
   k := 2
   l := 3
   e1[ 1 ] := 1.0
   e1[ 2 ] := 2.0
   e1[ 3 ] := 3.0
   FOR i := 1 TO 616 * nLoop
      P0()
   NEXT
   check += e1[ 1 ] + e1[ 2 ] + e1[ 3 ] + e1[ 4 ]
   x := 0.75
   FOR i := 1 TO 93 * nLoop
      x := Sqrt( Exp( Log( x ) / t1 ) )
   NEXT
   check += x
   ? "whetstone", ElapsedMs() - tm, IIf( Abs( check - 30.95612841002271 ) < 0.000000001, "ok", "WRONG" )
   RETURN

STATIC PROCEDURE PA( e )
   LOCAL n := 0
   DO WHILE n < 6
      e[ 1 ] := ( e[ 1 ] + e[ 2 ] + e[ 3 ] - e[ 4 ] ) * t
      e[ 2 ] := ( e[ 1 ] + e[ 2 ] - e[ 3 ] + e[ 4 ] ) * t
      e[ 3 ] := ( e[ 1 ] - e[ 2 ] + e[ 3 ] + e[ 4 ] ) * t
      e[ 4 ] := ( -e[ 1 ] + e[ 2 ] + e[ 3 ] + e[ 4 ] ) / t2
      n++
   ENDDO
   RETURN

STATIC PROCEDURE P0()
   e1[ j ] := e1[ k ]
   e1[ k ] := e1[ l ]
   e1[ l ] := e1[ j ]
   RETURN

STATIC PROCEDURE P3( x, y, z )
   LOCAL x1 := x, y1 := y
   x1 := t * ( x1 + y1 )
   y1 := t * ( x1 + y1 )
   z := ( x1 + y1 ) / t2
   RETURN
