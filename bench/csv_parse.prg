// CSV parsed byte by byte: a 40,000-line text (about 2 MB) of 5 fields, some quoted with commas, doubled quotes and
// line breaks inside, read by a state machine into an array of records; numbers read with Val(); 6 rounds.
STATIC s_nSeed := 1999

PROCEDURE Main()
   LOCAL t, c := "", cLine, i, r, aRecs, a, n := 0, nQuoted := 0, ok
   FOR i := 1 TO 40000
      cLine := LTrim( Str( i ) ) + "," + MakeField() + "," + LTrim( Str( Rand( 100000 ) ) ) + "," + ;
         MakeField() + "," + Replicate( "w", Rand( 10 ) ) + Chr( 10 )
      c += cLine
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 6
      aRecs := ParseCsv( c )
      FOR EACH a IN aRecs
         n += Val( a[ 1 ] ) + Val( a[ 3 ] ) + Len( a[ 2 ] ) + Len( a[ 4 ] ) + Len( a )
         IF Chr( 10 ) $ a[ 4 ] .OR. ( Chr( 34 ) $ a[ 2 ] )
            nQuoted++
         ENDIF
      NEXT
   NEXT
   ok := Len( aRecs ) == 40000 .AND. n == 16750579326 .AND. nQuoted == 73038
   ? "csv_parse", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION ParseCsv( c )
   LOCAL aRecs := {}, aRec := {}, cField := "", lQuoted := .F., i := 1, nLen := Len( c ), x
   DO WHILE i <= nLen
      x := SubStr( c, i, 1 )
      IF lQuoted
         IF x == Chr( 34 )
            IF SubStr( c, i + 1, 1 ) == Chr( 34 )
               cField += x
               i++
            ELSE
               lQuoted := .F.
            ENDIF
         ELSE
            cField += x
         ENDIF
      ELSEIF x == Chr( 34 )
         lQuoted := .T.
      ELSEIF x == ","
         AAdd( aRec, cField )
         cField := ""
      ELSEIF x == Chr( 10 )
         AAdd( aRec, cField )
         AAdd( aRecs, aRec )
         aRec := {}
         cField := ""
      ELSE
         cField += x
      ENDIF
      i++
   ENDDO
   RETURN aRecs

// A field: plain letters, or quoted with a comma, a doubled quote or a line break inside.
STATIC FUNCTION MakeField()
   LOCAL k := Rand( 6 ), w := Replicate( Chr( 97 + Rand( 26 ) ), 3 + Rand( 12 ) )
   IF k == 0
      RETURN Chr( 34 ) + w + ", " + w + Chr( 34 )
   ELSEIF k == 1
      RETURN Chr( 34 ) + "say " + Chr( 34 ) + Chr( 34 ) + w + Chr( 34 ) + Chr( 34 ) + Chr( 34 )
   ELSEIF k == 2
      RETURN Chr( 34 ) + w + Chr( 10 ) + w + Chr( 34 )
   ENDIF
   RETURN w

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
