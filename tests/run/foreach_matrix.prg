// FOR EACH over every kind of collection, and each inside each, at -O0 and -O2 (run0.*): a string, an array, a hash,
// a generator, a lazy Enumerable sequence (a string's Select()) and an object with GetEnumerator(); for each of the
// 36 pairs the values the inner loop sees, in order. Then DESCEND over the built-in collections, EXIT and LOOP in an
// inner loop, the loop variables captured by a codeblock and read after both loops, a loop over the collection of
// the loop around it, and chains of Enumerable methods over strings, arrays and hashes (one gone through twice).
PROCEDURE Main()
   LOCAL aKinds := { "string", "array", "hash", "generator", "sequence", "object" }, cOuter, cInner, x, y, c, b
   FOR EACH cOuter IN aKinds
      c := ""
      FOR EACH cInner IN aKinds
         c += Left( cInner, 1 ) + ":"
         FOR EACH x IN Make( cOuter )
            FOR EACH y IN Make( cInner )
               c += Show( x ) + Show( y ) + " "
            NEXT
         NEXT
      NEXT
      ? PadR( cOuter, 9 ), c
   NEXT
   c := ""
   FOR EACH x IN "abc" DESCEND
      FOR EACH y IN { 1, 2 } DESCEND
         c += x + Str( y, 1 )
      NEXT
   NEXT
   FOR EACH x IN { "k" => 1, "m" => 2 } DESCEND
      c += " " + Str( x, 1 )
   NEXT
   ? "descend:", c
   c := ""
   FOR EACH x IN "abcd"
      FOR EACH y IN { 1, 2, 3, 4 }
         IF y == 2
            LOOP
         ENDIF
         IF y == 4
            EXIT
         ENDIF
         c += x + Str( y, 1 )
      NEXT
      IF x == "c"
         EXIT
      ENDIF
   NEXT
   ? "exit and loop:", c, x, y
   b := {|| x + Str( y, 1 ) }
   c := ""
   FOR EACH x IN "pq"
      FOR EACH y IN { 7, 8 }
         c += Eval( b ) + " "
      NEXT
   NEXT
   ? "captured:", c, ValType( x ), ValType( y )
   c := ""
   FOR EACH x IN { "ab", "cde", "" }
      FOR EACH y IN x
         c += y + Str( Len( x ), 1 )
      NEXT
      c += "/"
   NEXT
   ? "the outer's element:", c
   ? "chains:", ;
      ToText( "Hello World":Where( {| ch | ch != " " } ):Select( {| ch | Upper( ch ) } ):Distinct():ToArray() ), ;
      "abcabc":Count( {| ch | ch == "b" } ), { 3, 1, 2 }:OrderBy( {| n | n } ):Select( {| n | n * 10 } ):Sum(), ;
      ToText( "xyz":Zip( { 1, 2, 3 }, {| ch, n | ch + Str( n, 1 ) } ):Reverse():ToArray() ), ;
      { "a" => 1, "b" => 2 }:Select( {| n | n * 2 } ):Sum(), "banana":Distinct():Count()
   x := "seq":Select( {| ch | ch + ch } )
   c := ""
   FOR EACH y IN x
      c += y
   NEXT
   FOR EACH y IN x
      c += "|" + y
   NEXT
   ? "twice:", c
RETURN

// Two values of each kind of collection.
STATIC FUNCTION Make( cKind )
   SWITCH cKind
   CASE "string"
      RETURN "st"
   CASE "array"
      RETURN { 1, 2 }
   CASE "hash"
      RETURN { "h" => 3, "i" => 4 }
   CASE "generator"
      RETURN Gen()
   CASE "sequence"
      RETURN "uv":Select( {| ch | Upper( ch ) } )
   ENDSWITCH
RETURN Pair():New()

STATIC FUNCTION Gen()
   YIELD 5
   YIELD 6
RETURN

STATIC FUNCTION Show( x )
RETURN IIf( ValType( x ) == "N", LTrim( Str( x ) ), x )

STATIC FUNCTION ToText( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += Show( x ) + ","
   NEXT
RETURN c

CLASS Pair
   METHOD New() INLINE Self
   METHOD GetEnumerator()
ENDCLASS

METHOD GetEnumerator() CLASS Pair
   YIELD "o"
   YIELD "p"
RETURN
