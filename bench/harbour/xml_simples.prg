// bench/xml_simples.prg in Harbour (hb_ATokens() where xbase has Split).
// Simple XML: a catalog of 50,000 books { title, author, price, year } (titles and authors with & < > and ' in them)
// made from a fixed seed, then timed: written as one document (the text escaped), split into elements with Split, and
// read back (each child's text found with At between its tags, the id attribute, the entities decoded with StrTran,
// the price with Val). The values read back add up as made.
STATIC nSeed := 5150

PROCEDURE Main()
   LOCAL aWords := { "Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter", ;
                     "Red", "Old" }
   LOCAL aAuthors := { "Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil" }
   LOCAL aBooks := Array( 50000 ), t, i, k, n, b, c, cBook, aParts, nCents := 0, nAmp := 0, nYears := 0, nEva := 0
   LOCAL cTitle, c12345, nIds := 0, ok
   FOR i := 1 TO Len( aBooks )
      n := Rand( 2 ) + 2
      cTitle := ""
      FOR k := 1 TO n
         cTitle += IIf( k > 1, " ", "" ) + aWords[ Rand( 10 ) + 1 ]
      NEXT
      aBooks[ i ] := { cTitle, aAuthors[ Rand( 5 ) + 1 ], Rand( 10000 ) + 100, 1950 + Rand( 75 ) }
   NEXT
   t := ElapsedMs()
   c := '<?xml version="1.0"?>' + Chr( 10 ) + "<catalog>" + Chr( 10 )
   FOR i := 1 TO Len( aBooks )
      b := aBooks[ i ]
      cBook := '<book id="' + LTrim( Str( i ) ) + '"><title>' + Escape( b[ 1 ] ) + "</title><author>" + ;
               Escape( b[ 2 ] ) + "</author><price>" + LTrim( Str( b[ 3 ] / 100, 10, 2 ) ) + "</price><year>" + ;
               Str( b[ 4 ], 4 ) + "</year></book>" + Chr( 10 )
      c += cBook
   NEXT
   c += "</catalog>" + Chr( 10 )
   ok := Len( c ) == 6276273
   aParts := hb_ATokens( c, "<book " )
   FOR i := 2 TO Len( aParts )
      cBook := aParts[ i ]
      nIds += Val( SubStr( cBook, 5 ) )
      cTitle := Unescape( Inner( cBook, "title" ) )
      IF "&" $ cTitle
         nAmp++
      ENDIF
      IF i == 12346
         c12345 := cTitle
      ENDIF
      IF Unescape( Inner( cBook, "author" ) ) == "Eva O'Neil"
         nEva++
      ENDIF
      nCents += Round( Val( Inner( cBook, "price" ) ) * 100, 0 )
      nYears += Val( Inner( cBook, "year" ) )
   NEXT
   ok := ok .AND. Len( aParts ) == 50001 .AND. nIds == 50000 * 50001 / 2 .AND. nCents == 255248286 .AND. ;
         nAmp == 21225 .AND. nYears == 99351507 .AND. nEva == 10152 .AND. c12345 == "Winter Glass Tom & Jerry"
   ? "xml_simples", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

// The text between <cTag> and </cTag> in c.
STATIC FUNCTION Inner( c, cTag )
   LOCAL nStart := At( "<" + cTag + ">", c ) + Len( cTag ) + 2
RETURN SubStr( c, nStart, At( "</" + cTag + ">", c ) - nStart )

STATIC FUNCTION Escape( c )
RETURN StrTran( StrTran( StrTran( StrTran( c, "&", "&amp;" ), "<", "&lt;" ), ">", "&gt;" ), "'", "&apos;" )

STATIC FUNCTION Unescape( c )
RETURN StrTran( StrTran( StrTran( StrTran( c, "&apos;", "'" ), "&gt;", ">" ), "&lt;", "<" ), "&amp;", "&" )

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
