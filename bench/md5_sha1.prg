// MD5 and SHA-1 in xBase: 32-bit words in integers, masked after each sum, rotations from the shift operators, bytes
// read with Asc/SubStr. A 200,000-byte buffer from an LCG is hashed 40 times with each, padded as the standards say;
// both are first checked against the published digests of "" and "abc", and the buffer's against md5sum/sha1sum.
#define MASK 0xFFFFFFFF

PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 200000, seed := 7, data := "", i, rep, ok
   ok := Md5Of( "" ) == "d41d8cd98f00b204e9800998ecf8427e" .AND. Md5Of( "abc" ) == "900150983cd24fb0d6963f7d28e17f72" ;
      .AND. Sha1Of( "" ) == "da39a3ee5e6b4b0d3255bfef95601890afd80709" ;
      .AND. Sha1Of( "abc" ) == "a9993e364706816aba3e25717850c26c9cd0d89d"
   FOR i := 1 TO n
      seed := ( seed * 1103515245 + 12345 ) & 0x7FFFFFFF
      data += Chr( ( seed >> 16 ) & 0xFF )
   NEXT
   FOR rep := 1 TO 40
      ok := ok .AND. Md5Of( data ) == "9caadd7dbbe1174da261d335b92f85fb" ;
         .AND. Sha1Of( data ) == "cec3d79455d923d234a1a40741be135a4ba980e2"
   NEXT
   ? "md5_sha1", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION RotL( x, s )
   RETURN ( ( x << s ) | ( x >> ( 32 - s ) ) ) & MASK

// The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes, the least significant
// first when lLittle.
STATIC FUNCTION Pad( s, lLittle )
   LOCAL n := Len( s ), nLen := Int( ( n + 8 ) / 64 ) * 64 + 64, cBits := "", i, b
   FOR i := 0 TO 7
      b := Chr( ( ( n * 8 ) >> ( 8 * i ) ) & 0xFF )
      cBits := IIf( lLittle, cBits + b, b + cBits )
   NEXT
   RETURN s + Chr( 0x80 ) + Replicate( Chr( 0 ), nLen - n - 9 ) + cBits

// The words as lowercase hexadecimal, each word's bytes the least significant first when lLittle.
STATIC FUNCTION Hex( aW, lLittle )
   LOCAL cOut := "", w, b, nByte
   FOR EACH w IN aW
      FOR b := 0 TO 3
         nByte := ( w >> IIf( lLittle, 8 * b, 24 - 8 * b ) ) & 0xFF
         cOut += SubStr( "0123456789abcdef", ( nByte >> 4 ) + 1, 1 )
         cOut += SubStr( "0123456789abcdef", ( nByte & 15 ) + 1, 1 )
      NEXT
   NEXT
   RETURN cOut

STATIC FUNCTION Md5Of( s )
   STATIC aK := { ;
      0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501, ;
      0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821, ;
      0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8, ;
      0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A, ;
      0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70, ;
      0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665, ;
      0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1, ;
      0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391 }
   STATIC aR := { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 }
   LOCAL p := Pad( s, .T. ), h := { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 }, m := Array( 16 )
   LOCAL nBlk, j, q, i, a, b, c, d, f, g, nTmp
   FOR nBlk := 0 TO Len( p ) - 1 STEP 64
      FOR j := 0 TO 15
         q := nBlk + 4 * j
         m[ j + 1 ] := Asc( SubStr( p, q + 1, 1 ) ) | ( Asc( SubStr( p, q + 2, 1 ) ) << 8 ) | ;
            ( Asc( SubStr( p, q + 3, 1 ) ) << 16 ) | ( Asc( SubStr( p, q + 4, 1 ) ) << 24 )
      NEXT
      a := h[ 1 ]
      b := h[ 2 ]
      c := h[ 3 ]
      d := h[ 4 ]
      FOR i := 0 TO 63
         IF i < 16
            f := ( b & c ) | ( ( b ^^ MASK ) & d )
            g := i
         ELSEIF i < 32
            f := ( d & b ) | ( ( d ^^ MASK ) & c )
            g := ( 5 * i + 1 ) & 15
         ELSEIF i < 48
            f := b ^^ c ^^ d
            g := ( 3 * i + 5 ) & 15
         ELSE
            f := c ^^ ( b | ( d ^^ MASK ) )
            g := ( 7 * i ) & 15
         ENDIF
         nTmp := d
         d := c
         c := b
         b := ( b + RotL( ( a + f + aK[ i + 1 ] + m[ g + 1 ] ) & MASK, aR[ ( i >> 4 ) * 4 + ( i & 3 ) + 1 ] ) ) & MASK
         a := nTmp
      NEXT
      h[ 1 ] := ( h[ 1 ] + a ) & MASK
      h[ 2 ] := ( h[ 2 ] + b ) & MASK
      h[ 3 ] := ( h[ 3 ] + c ) & MASK
      h[ 4 ] := ( h[ 4 ] + d ) & MASK
   NEXT
   RETURN Hex( h, .T. )

STATIC FUNCTION Sha1Of( s )
   LOCAL p := Pad( s, .F. ), h := { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 }, w := Array( 80 )
   LOCAL nBlk, j, q, i, a, b, c, d, e, f, k, nTmp
   FOR nBlk := 0 TO Len( p ) - 1 STEP 64
      FOR j := 0 TO 15
         q := nBlk + 4 * j
         w[ j + 1 ] := ( Asc( SubStr( p, q + 1, 1 ) ) << 24 ) | ( Asc( SubStr( p, q + 2, 1 ) ) << 16 ) | ;
            ( Asc( SubStr( p, q + 3, 1 ) ) << 8 ) | Asc( SubStr( p, q + 4, 1 ) )
      NEXT
      FOR j := 17 TO 80
         w[ j ] := RotL( w[ j - 3 ] ^^ w[ j - 8 ] ^^ w[ j - 14 ] ^^ w[ j - 16 ], 1 )
      NEXT
      a := h[ 1 ]
      b := h[ 2 ]
      c := h[ 3 ]
      d := h[ 4 ]
      e := h[ 5 ]
      FOR i := 0 TO 79
         IF i < 20
            f := ( b & c ) | ( ( b ^^ MASK ) & d )
            k := 0x5A827999
         ELSEIF i < 40
            f := b ^^ c ^^ d
            k := 0x6ED9EBA1
         ELSEIF i < 60
            f := ( b & c ) | ( b & d ) | ( c & d )
            k := 0x8F1BBCDC
         ELSE
            f := b ^^ c ^^ d
            k := 0xCA62C1D6
         ENDIF
         nTmp := ( RotL( a, 5 ) + f + e + k + w[ i + 1 ] ) & MASK
         e := d
         d := c
         c := RotL( b, 30 )
         b := a
         a := nTmp
      NEXT
      h[ 1 ] := ( h[ 1 ] + a ) & MASK
      h[ 2 ] := ( h[ 2 ] + b ) & MASK
      h[ 3 ] := ( h[ 3 ] + c ) & MASK
      h[ 4 ] := ( h[ 4 ] + d ) & MASK
      h[ 5 ] := ( h[ 5 ] + e ) & MASK
   NEXT
   RETURN Hex( h, .F. )
