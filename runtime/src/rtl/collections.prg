// Collections: the Enumerable class, which every collection inherits, and the classes of the built-in types (SCALAR
// CLASS), so that arrays, strings, hashes and the other values answer messages ({ 1, 2, 3 }:Select( {| x | x * 2 } ),
// "abc":Upper()) while Len( a ), AAdd( a, x ) and the rest stay as they are. Enumerable works with FOR EACH over
// Self: arrays, strings and hashes keep their own paths, an object goes through its enumerator (GetEnumerator(), or
// MoveNext() and Current), and a generator's enumerator answers Enumerable's messages too. The methods that make a
// sequence (Select, Where, Take, Skip) are generators: they run as the result is gone through.

// The methods of every collection. A class that inherits it only needs GetEnumerator() (a method with YIELD is
// the simplest one).
CLASS Enumerable
   METHOD Select( bMap )
   METHOD Where( bTest )
   METHOD Take( nCount )
   METHOD Skip( nCount )
   METHOD ToArray()
   METHOD Count( bTest )
   METHOD Any( bTest )
   METHOD All( bTest )
   METHOD First( bTest )
   METHOD Contains( xValue )
   METHOD Aggregate( bFunc, xSeed )
   METHOD Sum( bValue )
   METHOD ForEach( bBlock )
ENDCLASS

// The elements of a built-in collection, one by one, as FOR EACH gives them: its GetEnumerator().
STATIC FUNCTION Elements( xCollection )
   LOCAL x
   FOR EACH x IN xCollection
      YIELD x
   NEXT
RETURN

// Each element through bMap( element, position ).
METHOD Select( bMap ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      YIELD Eval( bMap, x, ++n )
   NEXT
RETURN

// The elements for which bTest( element, position ) is .T.
METHOD Where( bTest ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      IF Eval( bTest, x, ++n )
         YIELD x
      ENDIF
   NEXT
RETURN

// The first nCount elements.
METHOD Take( nCount ) CLASS Enumerable
   LOCAL x, n := 0
   IF nCount > 0
      FOR EACH x IN Self
         YIELD x
         IF ++n >= nCount
            EXIT
         ENDIF
      NEXT
   ENDIF
RETURN

// The elements after the first nCount.
METHOD Skip( nCount ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      IF ++n > nCount
         YIELD x
      ENDIF
   NEXT
RETURN

METHOD ToArray() CLASS Enumerable
   LOCAL x, a := {}
   FOR EACH x IN Self
      AAdd( a, x )
   NEXT
RETURN a

// How many elements there are (for which bTest( element ) is .T., with bTest).
METHOD Count( bTest ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      IF bTest == NIL .OR. Eval( bTest, x )
         n++
      ENDIF
   NEXT
RETURN n

// Whether there is an element (for which bTest( element ) is .T., with bTest).
METHOD Any( bTest ) CLASS Enumerable
   LOCAL x
   FOR EACH x IN Self
      IF bTest == NIL .OR. Eval( bTest, x )
         RETURN .T.
      ENDIF
   NEXT
RETURN .F.

// Whether bTest( element ) is .T. for every element (.T. when there is none).
METHOD All( bTest ) CLASS Enumerable
   LOCAL x
   FOR EACH x IN Self
      IF ! Eval( bTest, x )
         RETURN .F.
      ENDIF
   NEXT
RETURN .T.

// The first element (for which bTest( element ) is .T., with bTest); NIL when there is none.
METHOD First( bTest ) CLASS Enumerable
   LOCAL x
   FOR EACH x IN Self
      IF bTest == NIL .OR. Eval( bTest, x )
         RETURN x
      ENDIF
   NEXT
RETURN NIL

// Whether an element is exactly equal (==) to xValue.
METHOD Contains( xValue ) CLASS Enumerable
   LOCAL x
   FOR EACH x IN Self
      IF ValType( x ) == ValType( xValue ) .AND. x == xValue
         RETURN .T.
      ENDIF
   NEXT
RETURN .F.

// bFunc( accumulated, element ) over the elements, from xSeed (without it, from the first element).
METHOD Aggregate( bFunc, xSeed ) CLASS Enumerable
   LOCAL x, lFirst := PCount() < 2
   FOR EACH x IN Self
      IF lFirst
         xSeed := x
         lFirst := .F.
      ELSE
         xSeed := Eval( bFunc, xSeed, x )
      ENDIF
   NEXT
RETURN xSeed

// The sum of the elements (of bValue( element ), with bValue); 0 when there is none.
METHOD Sum( bValue ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      n += IIF( bValue == NIL, x, Eval( bValue, x ) )
   NEXT
RETURN n

// bBlock( element, position ) for each element; Self.
METHOD ForEach( bBlock ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      Eval( bBlock, x, ++n )
   NEXT
RETURN Self

// Arrays: the array functions as messages; Self changes in place and comes back where that reads naturally.
SCALAR CLASS ARRAY INHERIT Enumerable
   METHOD GetEnumerator() INLINE Elements( Self )
   METHOD Len() INLINE Len( Self )
   METHOD Count( bTest ) INLINE IIF( bTest == NIL, Len( Self ), ::Super:Count( bTest ) )
   METHOD Add( xValue ) INLINE AAdd( Self, xValue ), Self
   METHOD Insert( nPos, xValue ) INLINE ASize( Self, Len( Self ) + 1 ), AIns( Self, nPos ), Self[ nPos ] := xValue, ;
      Self
   METHOD Delete( nPos ) INLINE ADel( Self, nPos ), ASize( Self, Len( Self ) - 1 ), Self
   METHOD Resize( nLen ) INLINE ASize( Self, nLen )
   METHOD Fill( xValue, nStart, nCount ) INLINE AFill( Self, xValue, nStart, nCount )
   METHOD Sort( bOrder ) INLINE ASort( Self, , , bOrder )
   METHOD Scan( xSearch, nStart, nCount ) INLINE AScan( Self, xSearch, nStart, nCount )
   METHOD Eval( bBlock, nStart, nCount ) INLINE AEval( Self, bBlock, nStart, nCount )
   METHOD Clone() INLINE AClone( Self )
   METHOD Tail() INLINE ATail( Self )
   METHOD ToArray() INLINE AClone( Self )
ENDCLASS

// Strings: their characters are the elements.
SCALAR CLASS CHARACTER INHERIT Enumerable
   METHOD GetEnumerator() INLINE Elements( Self )
   METHOD Len() INLINE Len( Self )
   METHOD Count( bTest ) INLINE IIF( bTest == NIL, Len( Self ), ::Super:Count( bTest ) )
   METHOD Upper() INLINE Upper( Self )
   METHOD Lower() INLINE Lower( Self )
   METHOD Trim() INLINE Trim( Self )
   METHOD LTrim() INLINE LTrim( Self )
   METHOD RTrim() INLINE RTrim( Self )
   METHOD AllTrim() INLINE AllTrim( Self )
   METHOD Left( nLen ) INLINE Left( Self, nLen )
   METHOD Right( nLen ) INLINE Right( Self, nLen )
   METHOD SubStr( nStart, nLen ) INLINE IIF( nLen == NIL, SubStr( Self, nStart ), SubStr( Self, nStart, nLen ) )
   METHOD At( cSearch ) INLINE At( cSearch, Self )
   METHOD Contains( cSearch ) INLINE cSearch $ Self
   METHOD StartsWith( cStart ) INLINE Left( Self, Len( cStart ) ) == cStart
   METHOD EndsWith( cEnd ) INLINE Right( Self, Len( cEnd ) ) == cEnd
   METHOD Replicate( nCount ) INLINE Replicate( Self, nCount )
   METHOD Split( cSeparator )
ENDCLASS

// The pieces between the separators (a space without one); with an empty separator, the characters.
METHOD Split( cSeparator ) CLASS CHARACTER
   LOCAL a := {}, cRest := Self, n
   IF cSeparator == NIL
      cSeparator := " "
   ENDIF
   IF Len( cSeparator ) == 0
      RETURN ::ToArray()
   ENDIF
   DO WHILE ( n := At( cSeparator, cRest ) ) > 0
      AAdd( a, Left( cRest, n - 1 ) )
      cRest := SubStr( cRest, n + Len( cSeparator ) )
   ENDDO
   AAdd( a, cRest )
RETURN a

// Hashes: their values are the elements.
SCALAR CLASS HASH INHERIT Enumerable
   METHOD GetEnumerator() INLINE Elements( Self )
   METHOD Len() INLINE Len( Self )
   METHOD Count( bTest ) INLINE IIF( bTest == NIL, Len( Self ), ::Super:Count( bTest ) )
   METHOD Keys() INLINE HKeys( Self )
   METHOD Values() INLINE HValues( Self )
   METHOD HasKey( xKey ) INLINE HHasKey( Self, xKey )
   METHOD Get( xKey, xDefault ) INLINE HGetDef( Self, xKey, xDefault )
   METHOD Set( xKey, xValue ) INLINE HSet( Self, xKey, xValue ), Self
   METHOD Delete( xKey ) INLINE HDel( Self, xKey ), Self
ENDCLASS

SCALAR CLASS NUMERIC
   METHOD Str( nLen, nDec ) INLINE IIF( nLen == NIL, LTrim( Str( Self ) ), Str( Self, nLen, nDec ) )
   METHOD Round( nDec ) INLINE Round( Self, nDec )
   METHOD Int() INLINE Int( Self )
   METHOD Abs() INLINE Abs( Self )
ENDCLASS

SCALAR CLASS DATE
   METHOD Year() INLINE Year( Self )
   METHOD Month() INLINE Month( Self )
   METHOD Day() INLINE Day( Self )
   METHOD DoW() INLINE DoW( Self )
   METHOD DToS() INLINE DToS( Self )
ENDCLASS

SCALAR CLASS BLOCK
   METHOD Eval( ... ) INLINE Eval( Self, ... )
ENDCLASS

// AEval( aArray, bBlock, [nStart], [nCount] ): calls bBlock( element, position ) for each element of the range (the
// collection functions' rule: __RangeStart, __RangeCount) that still exists when its turn comes; returns the array.
// For a collection object (an Enumerable, a generator's enumerator), its elements from position nStart, nCount of
// them.
FUNCTION AEval( aArray, bBlock, nStart, nCount )
   LOCAL nLast, i, x
   IF ValType( bBlock ) != "B" .OR. ( ValType( aArray ) != "A" .AND. ValType( aArray ) != "O" )
      RETURN __ArgError( 2017 )
   ENDIF
   IF ValType( aArray ) == "O"
      nStart := IIF( nStart == NIL .OR. nStart < 1, 1, Int( nStart ) )
      i := 0
      FOR EACH x IN aArray
         IF ++i >= nStart
            IF nCount != NIL .AND. i - nStart >= nCount
               EXIT
            ENDIF
            Eval( bBlock, x, i )
         ENDIF
      NEXT
      RETURN aArray
   ENDIF
   nStart := __RangeStart( Len( aArray ), nStart )
   IF nStart == 0
      RETURN aArray
   ENDIF
   nLast := nStart + __RangeCount( Len( aArray ) - nStart + 1, nCount ) - 1
   FOR i := nStart TO nLast
      IF i > Len( aArray )
         EXIT
      ENDIF
      Eval( bBlock, aArray[ i ], i )
   NEXT
RETURN aArray

// AScan( aTarget, xSearch, [nStart], [nCount] ): the position of the first element in the range that matches xSearch
// (__ValueMatches: values of one comparable kind, with SET EXACT for strings, or two NILs; arrays, blocks and objects
// never match), or for which the block xSearch( element, position ) returns .T.; 0 when none does, or aTarget is not
// a collection (without xSearch, even NIL, nothing matches). A collection object's elements are counted from 1.
FUNCTION AScan( aTarget, xSearch, nStart, nCount )
   LOCAL i, x, lBlock := ValType( xSearch ) == "B"
   IF PCount() < 2
      RETURN 0
   ENDIF
   IF ValType( aTarget ) == "O"
      nStart := IIF( ValType( nStart ) != "N" .OR. nStart < 1, 1, Int( nStart ) )
      i := 0
      FOR EACH x IN aTarget
         IF ++i >= nStart
            IF ValType( nCount ) == "N" .AND. i - nStart >= nCount
               EXIT
            ENDIF
            IF lBlock
               x := Eval( xSearch, x, i )
               IF ValType( x ) == "L" .AND. x
                  RETURN i
               ENDIF
            ELSEIF __ValueMatches( x, xSearch )
               RETURN i
            ENDIF
         ENDIF
      NEXT
      RETURN 0
   ENDIF
   IF ValType( aTarget ) != "A"
      RETURN 0
   ENDIF
   nStart := __RangeStart( Len( aTarget ), nStart )
   IF nStart == 0
      RETURN 0
   ENDIF
   nCount := __RangeCount( Len( aTarget ) - nStart + 1, nCount )
   IF ! lBlock
      RETURN __AScanValue( aTarget, xSearch, nStart, nCount )
   ENDIF
   FOR i := nStart TO nStart + nCount - 1
      IF i > Len( aTarget ) // the block may shorten the array
         EXIT
      ENDIF
      x := Eval( xSearch, aTarget[ i ], i ) // only .T. finds
      IF ValType( x ) == "L" .AND. x
         RETURN i
      ENDIF
   NEXT
RETURN 0

// AFill( aTarget, xValue, [nStart], [nCount] ): the range from nStart, up to nCount elements (the rest of the array
// without it; none when it is 0 or less) takes the value; without a value nothing changes. Returns aTarget; anything
// but an array is BASE/6004.
FUNCTION AFill( aTarget, xValue, nStart, nCount )
   LOCAL i, nLast
   IF ValType( aTarget ) != "A"
      RETURN __ArgError( 6004 )
   ENDIF
   IF PCount() < 2
      RETURN aTarget
   ENDIF
   nStart := __RangeStart( Len( aTarget ), nStart )
   IF nStart == 0
      RETURN aTarget
   ENDIF
   nLast := Len( aTarget )
   IF ValType( nCount ) == "N"
      IF Int( nCount ) <= 0
         RETURN aTarget
      ENDIF
      nLast := Min( nLast, nStart + Int( nCount ) - 1 )
   ENDIF
   FOR i := nStart TO nLast
      aTarget[ i ] := xValue
   NEXT
RETURN aTarget

// ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] ): copies the range of aSource into aTarget from
// nTargetPos (missing or 0: 1; past the end or negative: the last position), as many elements as fit, in order.
// Returns aTarget (NIL unless both are arrays).
FUNCTION ACopy( aSource, aTarget, nStart, nCount, nTargetPos )
   LOCAL nTarget := 1, i
   IF ValType( aSource ) != "A" .OR. ValType( aTarget ) != "A"
      RETURN NIL
   ENDIF
   nStart := __RangeStart( Len( aSource ), nStart )
   IF nStart == 0 .OR. Len( aTarget ) == 0
      RETURN aTarget
   ENDIF
   nCount := __RangeCount( Len( aSource ) - nStart + 1, nCount )
   IF ValType( nTargetPos ) == "N"
      nTargetPos := Int( nTargetPos )
      IF nTargetPos < 0 .OR. nTargetPos > Len( aTarget )
         nTarget := Len( aTarget )
      ELSEIF nTargetPos > 0
         nTarget := nTargetPos
      ENDIF
   ENDIF
   nCount := Min( nCount, Len( aTarget ) - nTarget + 1 )
   FOR i := 0 TO nCount - 1
      aTarget[ nTarget + i ] := aSource[ nStart + i ]
   NEXT
RETURN aTarget

// HEval( hHash, bBlock, [nStart], [nCount] ): calls bBlock( key, value, position ) for each entry of the range still
// there when its turn comes; returns hHash. A hash and a block are required (BASE/1123).
FUNCTION HEval( hHash, bBlock, nStart, nCount )
   LOCAL nLast, nPos := 0, k, v
   IF ValType( hHash ) != "H" .OR. ValType( bBlock ) != "B"
      RETURN __ArgError( 1123 )
   ENDIF
   nStart := __RangeStart( Len( hHash ), nStart )
   IF nStart == 0
      RETURN hHash
   ENDIF
   nLast := nStart + __RangeCount( Len( hHash ) - nStart + 1, nCount ) - 1
   FOR EACH k => v IN hHash // each position checked against the hash as it is: entries the block deletes end it
      IF ++nPos > nLast
         EXIT
      ENDIF
      IF nPos >= nStart
         Eval( bBlock, k, v, nPos )
      ENDIF
   NEXT
RETURN hHash
