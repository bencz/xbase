// Each byte of each single-byte codepage to UTF8 and back (each of them gives a character no other of its bytes gives,
// so every byte comes back), and characters to each codepage from UTF8: ASCII, letters with accents, signs only some
// codepages have (the Euro, box drawing, Greek), one none has (written as "?"); then a long text both ways (CP437 has
// no "ã": it comes back as "?").
PROCEDURE Main()
   LOCAL cp, b, c, cBack, nSame, cOdd, cText, k, x
   FOR EACH cp IN { "CP437", "CP850", "CP1252", "ISO8859-1" }
      nSame := 0
      cOdd := ""
      FOR b := 0 TO 255
         c := CPConvert( Chr( b ), cp, "UTF8" )
         cBack := CPConvert( c, "UTF8", cp )
         IF cBack == Chr( b )
            nSame++
         ELSE
            cOdd += LTrim( Str( b ) ) + "->" + LTrim( Str( Asc( cBack ) ) ) + " "
         ENDIF
      NEXT
      ? cp, "round trips:", nSame, "others:", cOdd
      c := ""
      FOR EACH x IN { "A", "z", "é", "Ç", "ü", "€", "╬", "Σ", "ß", "★" }
         cBack := CPConvert( x, "UTF8", cp )
         c += x + "=" + iif( Len( cBack ) == 1, LTrim( Str( Asc( cBack ) ) ), ;
                             "len " + LTrim( Str( Len( cBack ) ) ) ) + " "
      NEXT
      ? cp, c
      cText := ""
      FOR k := 1 TO 200
         cText += "Ação é ü " + LTrim( Str( k ) )
      NEXT
      x := CPConvert( cText, "UTF8", cp )
      ? cp, "text:", Len( x ), CPConvert( x, cp, "UTF8" ) == cText
   NEXT
RETURN
