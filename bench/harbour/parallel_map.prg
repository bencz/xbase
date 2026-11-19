// bench/parallel_map.prg in Harbour: 1,000,000 numbers (1 to 1,000,000, from a fixed seed) mapped to their Collatz
// step counts by 64 threads (its threads where xbase has tasks), each mapping its slice into an array of its own; the
// slices are joined in order with ACopy, and the result checked: its sum, its maximum and where it is first, a running
// hash of the whole.
PROCEDURE Main()
   LOCAL n := 1000000, aIn := Array( n ), aOut := Array( n ), x := 777, i, t, aThreads := {}, nChunk, a, nPos := 1
   LOCAL nSum := 0, nMax := 0, nAt := 0, nChk := 0, th
   FOR i := 1 TO n
      x := x * 16807 % 2147483647
      aIn[ i ] := x % 1000000 + 1
   NEXT
   t := ElapsedMs()
   nChunk := n / 64
   FOR i := 1 TO n STEP nChunk
      AAdd( aThreads, hb_threadStart( {| nFrom | MapSlice( aIn, nFrom, nChunk ) }, i ) )
   NEXT
   FOR EACH th IN aThreads
      hb_threadJoin( th, @a )
      ACopy( a, aOut, 1, Len( a ), nPos )
      nPos += Len( a )
   NEXT
   FOR i := 1 TO n
      x := aOut[ i ]
      nSum += x
      IF x > nMax
         nMax := x
         nAt := i
      ENDIF
      nChk := ( nChk * 31 + x ) % 1000000007
   NEXT
   ? "parallel_map", ElapsedMs() - t, ;
     IIf( nPos == n + 1 .AND. nSum == 131359344 .AND. nMax == 524 .AND. nAt == 237731 .AND. nChk == 68222219, ;
          "ok", "WRONG" )
RETURN

STATIC FUNCTION MapSlice( aIn, nFrom, nCount )
   LOCAL a := Array( nCount ), i
   FOR i := 1 TO nCount
      a[ i ] := Steps( aIn[ nFrom + i - 1 ] )
   NEXT
RETURN a

STATIC FUNCTION Steps( x )
   LOCAL s := 0
   DO WHILE x != 1
      x := IIf( x % 2 == 0, x / 2, 3 * x + 1 )
      s++
   ENDDO
RETURN s
