// FOR EACH over objects, through the enumerator protocol: a collection answers GetEnumerator(), an enumerator answers
// MoveNext() (.T. while there is an element) and Current (a method, an ACCESS or a variable), and Reset() when it is
// given to FOR EACH itself. Also an enumerator given directly, FOR EACH k => v (k is the position), DESCEND (the
// elements gathered first), EXIT, nested loops over one collection, two collections in one loop (it ends with the
// shorter), the loop variable restored after the loop, a variable assigned in the loop (the collection keeps its
// elements), an error in MoveNext, and an object that is not a collection.
PROCEDURE Main()
   LOCAL r := TRange():New( 1, 4 ), x := "before", k, y, c := "", e
   FOR EACH x IN r
      c += Str( x, 2 )
   NEXT
   ? "range:", c, x
   c := ""
   FOR EACH x IN r:GetEnumerator()
      c += Str( x, 2 )
   NEXT
   ? "enumerator:", c
   c := ""
   FOR EACH k => x IN TRange():New( 10, 12 )
      c += Str( k, 2 ) + ":" + LTrim( Str( x ) )
   NEXT
   ? "keys:", c
   c := ""
   FOR EACH x IN r DESCEND
      c += Str( x, 2 )
   NEXT
   ? "descend:", c
   c := ""
   FOR EACH x IN r
      FOR EACH y IN r
         c += IIF( y > x, "", Str( x * 10 + y, 3 ) )
      NEXT
      IF x == 3
         EXIT
      ENDIF
   NEXT
   ? "nested:", c
   c := ""
   FOR EACH x, y IN r, { "a", "b" }
      c += LTrim( Str( x ) ) + y + " "
   NEXT
   ? "two:", c
   FOR EACH x IN r
      x := 0
   NEXT
   c := ""
   FOR EACH x IN r
      c += Str( x, 2 )
   NEXT
   ? "unchanged:", c
   c := ""
   FOR EACH x IN TWords():New( "one two three" )
      c += "[" + x + "]"
   NEXT
   ? "words:", c
   e := TRangeEnum():New( 1, 3 )
   FOR EACH x, y IN { "a" }, e
   NEXT
   c := ""
   FOR EACH y IN e
      c += Str( y, 2 )
   NEXT
   ? "not moved past the end of another:", c
   ErrorBlock( {| oErr | Break( oErr ) } )
   y := TForm():New()
   BEGIN SEQUENCE
      FOR EACH x IN y
      NEXT
   RECOVER USING e
      ? "not an enumerator:", e:subCode, y:data
   END SEQUENCE
   FOR EACH e IN { TBroken():New(), TNoEnum():New() }
      BEGIN SEQUENCE
         FOR EACH x IN e
            ? "not here"
         NEXT
      RECOVER USING y
         ? "error:", y:subCode, y:description, y:operation
      END SEQUENCE
   NEXT
RETURN

CLASS TRange
   VAR first, last
   METHOD New( a, b ) INLINE ::first := a, ::last := b, Self
   METHOD GetEnumerator() INLINE TRangeEnum():New( ::first, ::last )
ENDCLASS

CLASS TRangeEnum
   VAR at, last
   METHOD New( a, b ) INLINE ::at := a - 1, ::last := b, Self
   METHOD MoveNext() INLINE ++::at <= ::last
   ACCESS Current INLINE ::at
ENDCLASS

// An enumerator of its own words, with Reset and a Current variable.
CLASS TWords
   VAR text, Current
   VAR pos INIT 0
   METHOD New( c ) INLINE ::text := c, Self
   METHOD Reset() INLINE ::pos := 0
   METHOD MoveNext()
ENDCLASS

METHOD MoveNext() CLASS TWords
   LOCAL n
   IF ::pos > Len( ::text )
      RETURN .F.
   ENDIF
   n := At( " ", SubStr( ::text, ::pos + 1 ) )
   IF n == 0
      n := Len( ::text ) - ::pos + 1
   ENDIF
   ::Current := SubStr( ::text, ::pos + 1, n - 1 )
   ::pos += n
RETURN .T.

CLASS TBroken
   METHOD MoveNext() INLINE 1 + "x"
   ACCESS Current INLINE 0
ENDCLASS

CLASS TNoEnum
   VAR v
ENDCLASS

// Not an enumerator, though it has a Reset(): FOR EACH must not call it.
CLASS TForm
   VAR data INIT "kept"
   METHOD Reset() INLINE ::data := ""
ENDCLASS
