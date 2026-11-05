// CRC-32 of 20,000 bytes, 100 times: the bit functions, string building and Asc/SubStr.
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
         IF BitAnd( c, 1 ) == 1
            c := BitXor( 0xEDB88320, BitShr( c, 1 ) )
         ELSE
            c := BitShr( c, 1 )
         ENDIF
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
      crc := BitXor( table[ BitAnd( BitXor( crc, Asc( SubStr( data, i, 1 ) ) ), 255 ) + 1 ], BitShr( crc, 8 ) )
   NEXT
   RETURN BitXor( crc, 0xFFFFFFFF )
