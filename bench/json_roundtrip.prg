// JSON written and read back by xBase code (the runtime has no JSON functions): 20,000 records as hashes (an id, a
// name with quotes, backslashes, tabs and line feeds, an array of tags, a logical, a number, a null, a nested hash)
// made from a fixed seed, then timed: serialized to text, parsed back into hashes and arrays by a recursive descent
// parser, and serialized again. The two texts must be equal, and the values read back add up as made.
STATIC nSeed := 1618

PROCEDURE Main()
   LOCAL aData := Array( 20000 ), t, i, c1, c2, aBack, nPos := 1, h, nQty := 0, nActive := 0, nTags := 0
   LOCAL nNull := 0, nZip := 0, ok
   FOR i := 1 TO Len( aData )
      aData[ i ] := MakeRecord( i )
   NEXT
   t := ElapsedMs()
   c1 := ToJson( aData )
   aBack := Parse( c1, @nPos )
   c2 := ToJson( aBack )
   FOR EACH h IN aBack
      nQty += h[ "qty" ]
      nActive += IIf( h[ "active" ], 1, 0 )
      nTags += Len( h[ "tags" ] )
      nNull += IIf( h[ "parent" ] == NIL, 1, 0 )
      nZip += h[ "address" ][ "zip" ]
   NEXT
   ok := c1 == c2 .AND. Len( c1 ) == 2697804 .AND. nPos == Len( c1 ) + 1 .AND. Len( aBack ) == 20000 .AND. ;
         nQty == -88759 .AND. nActive == 9960 .AND. nTags == 29729 .AND. nNull == 6525 .AND. nZip == 1098378340 .AND. ;
         aBack[ 1 ][ "name" ] == "ykAeACczkccifkx" .AND. ;
         aBack[ 2 ][ "name" ] == 'Cdkk"\Ae' + Chr( 10 ) + "x" + Chr( 10 ) + "z" .AND. ;
         Left( c1, 33 ) == '[{"id":1,"name":"ykAeACczkccifkx"'
   ? "json_roundtrip", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION ToJson( x )
   LOCAL c, k, v, cItem, cSep := ""
   SWITCH ValType( x )
   CASE "H"
      c := "{"
      FOR EACH k => v IN x
         cItem := Quote( k ) + ":" + ToJson( v )
         c += cSep + cItem
         cSep := ","
      NEXT
      RETURN c + "}"
   CASE "A"
      c := "["
      FOR EACH v IN x
         cItem := ToJson( v )
         c += cSep + cItem
         cSep := ","
      NEXT
      RETURN c + "]"
   CASE "C"
      RETURN Quote( x )
   CASE "N"
      RETURN LTrim( Str( x, 20, 0 ) )
   CASE "L"
      RETURN IIf( x, "true", "false" )
   ENDSWITCH
RETURN "null"

STATIC FUNCTION Quote( c )
RETURN '"' + StrTran( StrTran( StrTran( StrTran( c, "\", "\\" ), '"', '\"' ), Chr( 10 ), "\n" ), Chr( 9 ), "\t" ) + '"'

// The value that starts at nPos, which it leaves after the value.
STATIC FUNCTION Parse( c, nPos )
   LOCAL ch := SubStr( c, nPos, 1 ), x, k
   DO CASE
   CASE ch == "{"
      x := { => }
      nPos++
      IF SubStr( c, nPos, 1 ) == "}"
         nPos++
         RETURN x
      ENDIF
      DO WHILE .T.
         k := ParseString( c, @nPos )
         nPos++
         x[ k ] := Parse( c, @nPos )
         ch := SubStr( c, nPos++, 1 )
         IF ch == "}"
            EXIT
         ENDIF
      ENDDO
   CASE ch == "["
      x := {}
      nPos++
      IF SubStr( c, nPos, 1 ) == "]"
         nPos++
         RETURN x
      ENDIF
      DO WHILE .T.
         AAdd( x, Parse( c, @nPos ) )
         ch := SubStr( c, nPos++, 1 )
         IF ch == "]"
            EXIT
         ENDIF
      ENDDO
   CASE ch == '"'
      x := ParseString( c, @nPos )
   CASE ch == "t"
      x := .T.
      nPos += 4
   CASE ch == "f"
      x := .F.
      nPos += 5
   CASE ch == "n"
      nPos += 4
   OTHERWISE
      k := nPos
      DO WHILE SubStr( c, nPos, 1 ) $ "-0123456789"
         nPos++
      ENDDO
      x := Val( SubStr( c, k, nPos - k ) )
   ENDCASE
RETURN x

STATIC FUNCTION ParseString( c, nPos )
   LOCAL cOut := "", nStart := ++nPos, ch
   DO WHILE ( ch := SubStr( c, nPos, 1 ) ) != '"'
      IF ch == "\"
         cOut += SubStr( c, nStart, nPos - nStart )
         ch := SubStr( c, nPos + 1, 1 )
         cOut += IIf( ch == "n", Chr( 10 ), IIf( ch == "t", Chr( 9 ), ch ) )
         nPos += 2
         nStart := nPos
      ELSE
         nPos++
      ENDIF
   ENDDO
   cOut += SubStr( c, nStart, nPos - nStart )
   nPos++
RETURN cOut

// A record, drawn from the seed in the order the checks' constants were computed.
STATIC FUNCTION MakeRecord( i )
   LOCAL cName := "", n, k, lActive, nQty, xParent, cCity, nZip, aTags := {}
   n := Rand( 11 ) + 5
   FOR k := 1 TO n
      cName += SubStr( 'abcdefghijk ABC"\' + Chr( 10 ) + Chr( 9 ) + "xyz", Rand( 22 ) + 1, 1 )
   NEXT
   n := Rand( 4 )
   lActive := Rand( 2 ) == 1
   nQty := Int( Rand( 2001 ) - 1000 )
   IF Rand( 3 ) != 0
      xParent := Int( Rand( i ) + 1 )
   ENDIF
   cCity := "City " + LTrim( Str( Rand( 50 ), 10, 0 ) )
   nZip := Int( Rand( 90000 ) + 10000 )
   FOR k := 1 TO n
      AAdd( aTags, "tag" + LTrim( Str( Rand( 100 ), 10, 0 ) ) )
   NEXT
RETURN { "id" => i, "name" => cName, "tags" => aTags, "active" => lActive, "qty" => nQty, "parent" => xParent, ;
         "address" => { "city" => cCity, "zip" => nZip } }

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
