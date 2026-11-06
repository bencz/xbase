// CRC-32 of 20,000 bytes, 100 times: the bit operators, string building and Asc/SubStr.
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
         c := ( c >> 1 ) ^^ ( 0xEDB88320 & -( c & 1 ) )
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
      crc := table[ ( ( crc ^^ Asc( SubStr( data, i, 1 ) ) ) & 0xFF ) + 1 ] ^^ ( crc >> 8 )
   NEXT
   RETURN crc ^^ 0xFFFFFFFF
