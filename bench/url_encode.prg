// Percent-encoding written in xBase: 20,000 strings of 20 to 80 bytes (letters, digits, blanks, punctuation and bytes
// above 127) encoded as in a URL query (unreserved bytes kept, a blank as +, any other byte as %XX) and decoded back;
// 8 rounds.
STATIC s_nSeed := 80

PROCEDURE Main()
   LOCAL t, a := Array( 20000 ), i, j, c, x, r, cEnc, n := 0, ok := .T.
   LOCAL cChars := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+" + Chr( 233 ) + Chr( 252 )
   FOR i := 1 TO Len( a )
      c := ""
      FOR j := 1 TO 20 + Rand( 61 )
         x := SubStr( cChars, Rand( Len( cChars ) ) + 1, 1 )
         c += x
      NEXT
      a[ i ] := c
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 8
      FOR EACH c IN a
         cEnc := UrlEncode( c )
         n += Len( cEnc ) + Asc( Right( cEnc, 1 ) )
         ok := ok .AND. UrlDecode( cEnc ) == c
      NEXT
   NEXT
   ok := ok .AND. n == 18744664 .AND. UrlEncode( "a b" + Chr( 38 ) + "c=" + Chr( 233 ) ) == "a+b%26c%3D%E9"
   ? "url_encode", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

#define HEX_DIGITS "0123456789ABCDEF"

STATIC FUNCTION UrlEncode( c )
   LOCAL cOut := "", i, x, n
   FOR i := 1 TO Len( c )
      x := SubStr( c, i, 1 )
      IF x $ "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~"
         cOut += x
      ELSEIF x == " "
         cOut += "+"
      ELSE
         n := Asc( x )
         cOut += "%" + SubStr( HEX_DIGITS, Int( n / 16 ) + 1, 1 ) + SubStr( HEX_DIGITS, n % 16 + 1, 1 )
      ENDIF
   NEXT
   RETURN cOut

STATIC FUNCTION UrlDecode( c )
   LOCAL cOut := "", i := 1, x, nLen := Len( c )
   DO WHILE i <= nLen
      x := SubStr( c, i, 1 )
      IF x == "%"
         cOut += Chr( ( At( SubStr( c, i + 1, 1 ), HEX_DIGITS ) - 1 ) * 16 + ;
            At( SubStr( c, i + 2, 1 ), HEX_DIGITS ) - 1 )
         i += 3
      ELSE
         cOut += IIf( x == "+", " ", x )
         i++
      ENDIF
   ENDDO
   RETURN cOut

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
