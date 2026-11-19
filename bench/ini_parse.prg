// INI files: a text of 400 sections of 25 keys (comments, blank lines, spaces around "=", quoted values, section names
// in either case) made from a fixed seed, then timed: parsed 80 times into a hash of sections, each a hash of keys
// (lines split with Split, trimmed, names lowered, quotes taken off); the numbers summed and values looked up.
STATIC nSeed := 7777

PROCEDURE Main()
   LOCAL aLines := {}, s, k, nSp, n, cVal, c := "", t, i, h, x, nSum, nQuoted := 0, ok := .T.
   FOR s := 1 TO 400
      AAdd( aLines, "; section " + LTrim( Str( s ) ) )
      AAdd( aLines, "[" + IIf( Rand( 2 ) == 1, "Section", "SECTION" ) + LTrim( Str( s ) ) + "]" )
      FOR k := 1 TO 25
         nSp := Rand( 3 )
         n := LTrim( Str( Rand( 100000 ), 10, 0 ) )
         cVal := IIf( k % 5 == 0, n, IIf( k % 5 == 1, '"text ' + n + '"', "w" + n ) )
         AAdd( aLines, IIf( nSp == 2, "  ", "" ) + "Key" + StrZero( k, 2 ) + IIf( nSp > 0, " = ", "=" ) + cVal )
         IF k % 7 == 0
            AAdd( aLines, "# comment" )
         ENDIF
      NEXT
      AAdd( aLines, "" )
   NEXT
   FOR EACH x IN aLines
      c += x + Chr( 10 )
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 80
      h := ParseIni( c, @nQuoted )
      nSum := 0
      FOR EACH x IN h
         FOR k := 5 TO 25 STEP 5
            nSum += Val( x[ "key" + StrZero( k, 2 ) ] )
         NEXT
      NEXT
      ok := ok .AND. Len( h ) == 400 .AND. nSum == 102092980 .AND. nQuoted == 2000 .AND. ;
            h[ "section77" ][ "key11" ] == "text 72557" .AND. Len( h[ "section400" ] ) == 25
   NEXT
   ok := ok .AND. Len( c ) == 181694
   ? "ini_parse", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

// The sections of the text, each a hash of its keys; nQuoted gets the count of quoted values.
STATIC FUNCTION ParseIni( c, nQuoted )
   LOCAL hIni := { => }, hSection, cLine, nEq, cKey, cVal
   nQuoted := 0
   FOR EACH cLine IN c:Split( Chr( 10 ) )
      cLine := AllTrim( cLine )
      IF Empty( cLine ) .OR. Left( cLine, 1 ) $ ";#"
         LOOP
      ENDIF
      IF Left( cLine, 1 ) == "["
         hSection := { => }
         hIni[ Lower( SubStr( cLine, 2, Len( cLine ) - 2 ) ) ] := hSection
      ELSEIF ( nEq := At( "=", cLine ) ) > 0
         cKey := Lower( RTrim( Left( cLine, nEq - 1 ) ) )
         cVal := LTrim( SubStr( cLine, nEq + 1 ) )
         IF Len( cVal ) >= 2 .AND. Left( cVal, 1 ) == '"' .AND. Right( cVal, 1 ) == '"'
            cVal := SubStr( cVal, 2, Len( cVal ) - 2 )
            nQuoted++
         ENDIF
         hSection[ cKey ] := cVal
      ENDIF
   NEXT
RETURN hIni

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
