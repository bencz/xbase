// bench/radix_sort.prg in Harbour (its bit functions where xbase has bit operators).
// Least-significant-digit radix sort, four passes of one byte each, of 300,000 31-bit integers from a linear
// congruential generator, between two arrays, 10 times with different seeds; the order and a weighted checksum are
// checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 300000, a := Array( n ), b := Array( n ), cnt := Array( 256 ), seed, i, k, d
   LOCAL rep, ok := .T., sum := 0, pos, shift, x, tmp
   FOR rep := 1 TO 10
      seed := rep + 30
      FOR i := 1 TO n
         seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
         a[ i ] := seed
      NEXT
      FOR d := 0 TO 3
         shift := d * 8
         AFill( cnt, 0 )
         FOR i := 1 TO n
            cnt[ hb_bitAnd( hb_bitShift( a[ i ], -shift ), 0xFF ) + 1 ]++
         NEXT
         pos := 1
         FOR k := 1 TO 256
            x := cnt[ k ]
            cnt[ k ] := pos
            pos += x
         NEXT
         FOR i := 1 TO n
            x := a[ i ]
            k := hb_bitAnd( hb_bitShift( x, -shift ), 0xFF ) + 1
            b[ cnt[ k ] ] := x
            cnt[ k ]++
         NEXT
         tmp := a
         a := b
         b := tmp
      NEXT
      FOR i := 2 TO n
         IF a[ i - 1 ] > a[ i ]
            ok := .F.
         ENDIF
      NEXT
      FOR i := 1 TO n
         sum += a[ i ] * ( hb_bitAnd( i, 7 ) + 1 )
      NEXT
   NEXT
   ? "radix_sort", ElapsedMs() - t, IIf( ok .AND. sum == 14501540638159929, "ok", "WRONG" )
   RETURN
