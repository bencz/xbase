// bench/crc32.prg in Harbour: CRC-32 of 20,000 bytes, 100 times, with its bit functions where xbase has operators.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), rep, ok := .T.
   FOR rep := 1 TO 100
      ok := ok .AND. Crc32Of20K() == 2954121016
   NEXT
   ? "crc32", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

FUNCTION Crc32Of20K()
   LOCAL table := Array( 256 ), data := "", chunk, i, k, c, crc
   FOR i := 0 TO 255
      c := i
      FOR k := 1 TO 8
         c := hb_bitXor( hb_bitShift( c, -1 ), hb_bitAnd( 0xEDB88320, -hb_bitAnd( c, 1 ) ) )
      NEXT
      table[ i + 1 ] := c
   NEXT
   FOR i := 0 TO 19999 STEP 100
      chunk := ""
      FOR k := i TO i + 99
         chunk := chunk + Chr( ( k * 31 + 7 ) % 251 )
      NEXT
      data := data + chunk
   NEXT
   crc := 0xFFFFFFFF
   FOR i := 1 TO Len( data )
      crc := hb_bitXor( table[ hb_bitAnd( hb_bitXor( crc, Asc( SubStr( data, i, 1 ) ) ), 0xFF ) + 1 ], ;
                        hb_bitShift( crc, -8 ) )
   NEXT
   RETURN hb_bitXor( crc, 0xFFFFFFFF )
