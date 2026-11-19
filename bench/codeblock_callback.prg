// Functions that take a block: Simpson's rule over 3 functions given as blocks (1,000,000 intervals each), and an
// insertion sort of 3,000 numbers that compares through a block, 10 times.
STATIC s_nSeed := 1

PROCEDURE Main()
   LOCAL t, aData := Array( 3000 ), a, i, r, nInt := 0, nCheck := 0, ok
   FOR i := 1 TO Len( aData )
      aData[ i ] := Rand( 100000 )
   NEXT
   t := ElapsedMs()
   nInt += Simpson( {| x | x * x }, 0, 3, 1000000 )
   nInt += Simpson( {| x | 1 / ( 1 + x * x ) }, 0, 1, 1000000 )
   nInt += Simpson( {| x | x * x * x - 2 * x }, -1, 2, 1000000 )
   FOR r := 1 TO 10
      a := AClone( aData )
      InsertionSort( a, IIf( r % 2 == 0, {| x, y | x < y }, {| x, y | x > y } ) )
      nCheck += a[ 1 ] + a[ 1500 ] * 2 + a[ 3000 ] * 3
   NEXT
   ok := Abs( nInt - 10.535398163397 ) < 0.000000001 .AND. nCheck == 2991620
   ? "codeblock_callback", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Simpson( bF, nA, nB, n )
   LOCAL h := ( nB - nA ) / n, nSum := Eval( bF, nA ) + Eval( bF, nB ), i
   FOR i := 1 TO n - 1
      nSum += Eval( bF, nA + i * h ) * IIf( i % 2 == 1, 4, 2 )
   NEXT
   RETURN nSum * h / 3

STATIC PROCEDURE InsertionSort( a, bLess )
   LOCAL i, j, x
   FOR i := 2 TO Len( a )
      x := a[ i ]
      j := i - 1
      DO WHILE j >= 1 .AND. Eval( bLess, x, a[ j ] )
         a[ j + 1 ] := a[ j ]
         j--
      ENDDO
      a[ j + 1 ] := x
   NEXT
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
