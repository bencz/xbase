// Base64 written in xBase: 1,200,000 bytes from a fixed-seed generator encoded (three bytes to four characters through
// a table) and decoded back; 3 rounds.
#define B64_TABLE "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

STATIC s_nSeed := 64

PROCEDURE Main()
   LOCAL t, c := "", x, i, r, cEnc, cDec, n := 0, ok := .T.
   FOR i := 1 TO 1200000
      x := Chr( Rand( 256 ) )
      c += x
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 3
      cEnc := Encode( Left( c, Len( c ) - r + 1 ) )
      cDec := Decode( cEnc )
      ok := ok .AND. cDec == Left( c, Len( c ) - r + 1 )
      n += Len( cEnc )
      FOR i := 1 TO Len( cEnc ) STEP 97
         n += Asc( SubStr( cEnc, i, 1 ) )
      NEXT
   NEXT
   ok := ok .AND. n == 9038319 .AND. Encode( "Ma" ) == "TWE=" .AND. Decode( "TQ==" ) == "M"
   ? "base64", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN


STATIC FUNCTION Encode( c )
   LOCAL cOut := "", nLen := Len( c ), i, n, nRest := Len( c ) % 3
   FOR i := 1 TO nLen - nRest STEP 3
      n := Asc( SubStr( c, i, 1 ) ) * 65536 + Asc( SubStr( c, i + 1, 1 ) ) * 256 + Asc( SubStr( c, i + 2, 1 ) )
      cOut += SubStr( B64_TABLE, Int( n / 262144 ) + 1, 1 ) + SubStr( B64_TABLE, Int( n / 4096 ) % 64 + 1, 1 ) + ;
         SubStr( B64_TABLE, Int( n / 64 ) % 64 + 1, 1 ) + SubStr( B64_TABLE, n % 64 + 1, 1 )
   NEXT
   IF nRest == 1
      n := Asc( SubStr( c, nLen, 1 ) )
      cOut += SubStr( B64_TABLE, Int( n / 4 ) + 1, 1 ) + SubStr( B64_TABLE, ( n % 4 ) * 16 + 1, 1 ) + "=="
   ELSEIF nRest == 2
      n := Asc( SubStr( c, nLen - 1, 1 ) ) * 256 + Asc( SubStr( c, nLen, 1 ) )
      cOut += SubStr( B64_TABLE, Int( n / 1024 ) + 1, 1 ) + SubStr( B64_TABLE, Int( n / 16 ) % 64 + 1, 1 ) + ;
         SubStr( B64_TABLE, ( n % 16 ) * 4 + 1, 1 ) + "="
   ENDIF
   RETURN cOut

STATIC FUNCTION Decode( c )
   STATIC s_aValue
   LOCAL cOut := "", nLen := Len( c ), i, n, nPad := 0
   IF s_aValue == NIL
      s_aValue := AFill( Array( 256 ), 0 )
      FOR i := 1 TO 64
         s_aValue[ Asc( SubStr( B64_TABLE, i, 1 ) ) + 1 ] := i - 1
      NEXT
   ENDIF
   IF Right( c, 2 ) == "=="
      nPad := 2
   ELSEIF Right( c, 1 ) == "="
      nPad := 1
   ENDIF
   FOR i := 1 TO nLen STEP 4
      n := s_aValue[ Asc( SubStr( c, i, 1 ) ) + 1 ] * 262144 + s_aValue[ Asc( SubStr( c, i + 1, 1 ) ) + 1 ] * 4096 + ;
         s_aValue[ Asc( SubStr( c, i + 2, 1 ) ) + 1 ] * 64 + s_aValue[ Asc( SubStr( c, i + 3, 1 ) ) + 1 ]
      cOut += Chr( Int( n / 65536 ) ) + Chr( Int( n / 256 ) % 256 ) + Chr( n % 256 )
   NEXT
   RETURN Left( cOut, Len( cOut ) - nPad )

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
