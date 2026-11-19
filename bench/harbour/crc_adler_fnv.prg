// bench/crc_adler_fnv.prg in Harbour (its bit functions where xbase has bit operators).
// Checksums of bytes: twenty buffers of 1,000,000 bytes from an LCG, built a character at a time, each hashed with
// CRC-32 (from a table), Adler-32 and FNV-1a (32 bits); the functions are first checked against their published
// values ("123456789", "Wikipedia", "a").
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1000000, seed := 42, total := 0, table := Array( 256 ), data, i, k, c, rep, ok
   FOR i := 0 TO 255
      c := i
      FOR k := 1 TO 8
         c := hb_bitXor( hb_bitShift( c, -1 ), hb_bitAnd( 0xEDB88320, -hb_bitAnd( c, 1 ) ) )
      NEXT
      table[ i + 1 ] := c
   NEXT
   ok := Crc32Of( table, "123456789" ) == 0xCBF43926 .AND. Adler32Of( "Wikipedia" ) == 0x11E60398 .AND. ;
      Fnv1aOf( "a" ) == 0xE40C292C
   FOR rep := 1 TO 20
      data := ""
      FOR i := 1 TO n
         seed := hb_bitAnd( seed * 1103515245 + 12345, 0x7FFFFFFF )
         data += Chr( hb_bitAnd( hb_bitShift( seed, -16 ), 0xFF ) )
      NEXT
      total += Crc32Of( table, data ) + Adler32Of( data ) + Fnv1aOf( data )
   NEXT
   ? "crc_adler_fnv", ElapsedMs() - t, IIf( ok .AND. total == 142519858956, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Crc32Of( table, s )
   LOCAL crc := 0xFFFFFFFF, i
   FOR i := 1 TO Len( s )
      crc := hb_bitXor( table[ hb_bitAnd( hb_bitXor( crc, Asc( SubStr( s, i, 1 ) ) ), 0xFF ) + 1 ], ;
                hb_bitShift( crc, -8 ) )
   NEXT
   RETURN hb_bitXor( crc, 0xFFFFFFFF )

STATIC FUNCTION Adler32Of( s )
   LOCAL a := 1, b := 0, i
   FOR i := 1 TO Len( s )
      a += Asc( SubStr( s, i, 1 ) )
      IF a >= 65521
         a -= 65521
      ENDIF
      b += a
      IF b >= 65521
         b -= 65521
      ENDIF
   NEXT
   RETURN hb_bitOr( hb_bitShift( b, 16 ), a )

STATIC FUNCTION Fnv1aOf( s )
   LOCAL h := 2166136261, i
   FOR i := 1 TO Len( s )
      h := hb_bitAnd( hb_bitXor( h, Asc( SubStr( s, i, 1 ) ) ) * 16777619, 0xFFFFFFFF )
   NEXT
   RETURN h
