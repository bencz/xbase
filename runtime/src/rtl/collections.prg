// Collections: the Enumerable class, which every collection inherits, and the classes of the built-in types (SCALAR
// CLASS), so that arrays, strings, hashes and the other values answer messages ({ 1, 2, 3 }:Select( {| x | x * 2 } ),
// "abc":Upper()) while Len( a ), AAdd( a, x ) and the rest stay as they are. Enumerable works with FOR EACH over
// Self: arrays, strings and hashes keep their own paths, an object goes through its enumerator (GetEnumerator(), or
// MoveNext() and Current), and a generator's enumerator answers Enumerable's messages too. The methods that make a
// sequence (Select, Where, Take, Skip, TakeWhile, SkipWhile, SelectMany, Concat, Zip, Distinct, Reverse) are
// generators: they run as the result is gone through.

// The methods of every collection. A class that inherits it only needs GetEnumerator() (a method with YIELD is
// the simplest one).
CLASS Enumerable
   METHOD Select( bMap )
   METHOD Where( bTest )
   METHOD Take( nCount )
   METHOD Skip( nCount )
   METHOD TakeWhile( bTest )
   METHOD SkipWhile( bTest )
   METHOD SelectMany( bMap )
   METHOD Concat( xOther )
   METHOD Zip( xOther, bCombine )
   METHOD Distinct( bKey )
   METHOD Reverse()
   METHOD OrderBy( bKey )
   METHOD OrderByDescending( bKey )
   METHOD GroupBy( bKey )
   METHOD ToHash( bKey, bValue )
   METHOD ToArray()
   METHOD Count( bTest )
   METHOD Any( bTest )
   METHOD All( bTest )
   METHOD First( bTest )
   METHOD Last( bTest )
   METHOD Contains( xValue )
   METHOD Aggregate( bFunc, xSeed )
   METHOD Sum( bValue )
   METHOD Min( bValue )
   METHOD Max( bValue )
   METHOD Average( bValue )
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

// The elements up to the first for which bTest( element, position ) is not .T.
METHOD TakeWhile( bTest ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      IF ! Eval( bTest, x, ++n )
         EXIT
      ENDIF
      YIELD x
   NEXT
RETURN

// The elements from the first for which bTest( element, position ) is not .T.
METHOD SkipWhile( bTest ) CLASS Enumerable
   LOCAL x, n := 0, lTaking := .F.
   FOR EACH x IN Self
      IF lTaking .OR. ! Eval( bTest, x, ++n )
         lTaking := .T.
         YIELD x
      ENDIF
   NEXT
RETURN

// The elements of each collection bMap( element, position ) gives, one after the other.
METHOD SelectMany( bMap ) CLASS Enumerable
   LOCAL x, y, n := 0
   FOR EACH x IN Self
      FOR EACH y IN Eval( bMap, x, ++n )
         YIELD y
      NEXT
   NEXT
RETURN

// The elements, then those of the collection xOther.
METHOD Concat( xOther ) CLASS Enumerable
   LOCAL x
   FOR EACH x IN Self
      YIELD x
   NEXT
   FOR EACH x IN xOther
      YIELD x
   NEXT
RETURN

// bCombine( element, other ) for the elements of Self and of the collection xOther taken in pairs (without bCombine,
// { element, other }), as long as both have one.
METHOD Zip( xOther, bCombine ) CLASS Enumerable
   LOCAL x, oOther := Elements( xOther )
   FOR EACH x IN Self
      IF ! oOther:MoveNext()
         EXIT
      ENDIF
      YIELD IIF( bCombine == NIL, { x, oOther:Current }, Eval( bCombine, x, oOther:Current ) )
   NEXT
RETURN

// The elements whose value (bKey( element ), with bKey) was not met before: equal by ==, a number, a string or a date
// found through a hash.
METHOD Distinct( bKey ) CLASS Enumerable
   LOCAL x, xKey, hSeen := { => }, aSeen := {}
   FOR EACH x IN Self
      xKey := IIF( bKey == NIL, x, Eval( bKey, x ) )
      IF ValType( xKey ) $ "CND"
         IF xKey $ hSeen
            LOOP
         ENDIF
         hSeen[ xKey ] := .T.
      ELSE
         IF AScan( aSeen, {| y | ValType( y ) == ValType( xKey ) .AND. y == xKey } ) > 0
            LOOP
         ENDIF
         AAdd( aSeen, xKey )
      ENDIF
      YIELD x
   NEXT
RETURN

// The elements from the last to the first (gathered when the first one is asked for).
METHOD Reverse() CLASS Enumerable
   LOCAL a := ::ToArray(), i
   FOR i := Len( a ) TO 1 STEP -1
      YIELD a[ i ]
   NEXT
RETURN

// An array of the elements ordered by bKey( element ) (by themselves without bKey), as ASort orders values; elements
// with equal keys keep their order.
METHOD OrderBy( bKey ) CLASS Enumerable
RETURN Ordered( Self, bKey, .F. )

// The same, from the greatest key to the smallest.
METHOD OrderByDescending( bKey ) CLASS Enumerable
RETURN Ordered( Self, bKey, .T. )

// A hash of bKey( element ) to the array of the elements with that key, in the order they come.
METHOD GroupBy( bKey ) CLASS Enumerable
   LOCAL x, xKey, h := { => }
   FOR EACH x IN Self
      xKey := Eval( bKey, x )
      IF xKey $ h
         AAdd( h[ xKey ], x )
      ELSE
         h[ xKey ] := { x }
      ENDIF
   NEXT
RETURN h

// A hash of bKey( element ) to bValue( element ) (the element without bValue); a key met again takes the new value.
METHOD ToHash( bKey, bValue ) CLASS Enumerable
   LOCAL x, h := { => }
   FOR EACH x IN Self
      h[ Eval( bKey, x ) ] := IIF( bValue == NIL, x, Eval( bValue, x ) )
   NEXT
RETURN h

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

// The last element (for which bTest( element ) is .T., with bTest); NIL when there is none.
METHOD Last( bTest ) CLASS Enumerable
   LOCAL x, xLast
   FOR EACH x IN Self
      IF bTest == NIL .OR. Eval( bTest, x )
         xLast := x
      ENDIF
   NEXT
RETURN xLast

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

// The smallest element (of bValue( element ), with bValue), by <; NIL when there is none.
METHOD Min( bValue ) CLASS Enumerable
RETURN Extreme( Self, bValue, .F. )

// The greatest element (of bValue( element ), with bValue), by >; NIL when there is none.
METHOD Max( bValue ) CLASS Enumerable
RETURN Extreme( Self, bValue, .T. )

// The mean of the elements (of bValue( element ), with bValue); NIL when there is none.
METHOD Average( bValue ) CLASS Enumerable
   LOCAL x, nSum := 0, n := 0
   FOR EACH x IN Self
      nSum += IIF( bValue == NIL, x, Eval( bValue, x ) )
      n++
   NEXT
RETURN IIF( n == 0, NIL, nSum / n )

// bBlock( element, position ) for each element; Self.
METHOD ForEach( bBlock ) CLASS Enumerable
   LOCAL x, n := 0
   FOR EACH x IN Self
      Eval( bBlock, x, ++n )
   NEXT
RETURN Self

// The smallest value of the elements of oCollection (of bValue( element ), with bValue), or with lGreatest the
// greatest; NIL when there is none.
STATIC FUNCTION Extreme( oCollection, bValue, lGreatest )
   LOCAL x, xValue, xBest, lFirst := .T.
   FOR EACH x IN oCollection // x stays unassigned: it is the element itself
      xValue := IIF( bValue == NIL, x, Eval( bValue, x ) )
      IF lFirst .OR. IIF( lGreatest, xValue > xBest, xValue < xBest )
         xBest := xValue
         lFirst := .F.
      ENDIF
   NEXT
RETURN xBest

// The elements of oCollection in an array, ordered by bKey( element ) (by themselves without bKey) as ASort orders
// values, or from the greatest key with lDescending; equal keys keep their order (ASort is stable).
STATIC FUNCTION Ordered( oCollection, bKey, lDescending )
   LOCAL aPairs := {}, x, i
   FOR EACH x IN oCollection
      AAdd( aPairs, { IIF( bKey == NIL, x, Eval( bKey, x ) ), x } )
   NEXT
   IF lDescending
      ASort( aPairs, , , {| a, b | __ValueLess( b[ 1 ], a[ 1 ] ) } )
   ELSE
      ASort( aPairs, , , {| a, b | __ValueLess( a[ 1 ], b[ 1 ] ) } )
   ENDIF
   FOR i := 1 TO Len( aPairs )
      aPairs[ i ] := aPairs[ i ][ 2 ]
   NEXT
RETURN aPairs

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

// The elements of a collection object from position nStart (1 when it is no number or less than 1), nCount of them
// (all when it is no number), one by one to bVisit( element, position ) until it returns .T.: that position, else 0.
STATIC FUNCTION VisitRange( oCollection, nStart, nCount, bVisit )
   LOCAL i := 0, x
   nStart := IIF( ValType( nStart ) != "N" .OR. nStart < 1, 1, Int( nStart ) )
   FOR EACH x IN oCollection
      IF ++i >= nStart
         IF ValType( nCount ) == "N" .AND. i - nStart >= nCount
            EXIT
         ENDIF
         IF Eval( bVisit, x, i )
            RETURN i
         ENDIF
      ENDIF
   NEXT
RETURN 0

// AEval( aArray, bBlock, [nStart], [nCount] ): calls bBlock( element, position ) for each element of the range (the
// collection functions' rule: __RangeStart, __RangeCount) that still exists when its turn comes; returns the array.
// For a collection object (an Enumerable, a generator's enumerator), its elements from position nStart, nCount of
// them (VisitRange); any other value is BASE/2017.
FUNCTION AEval( aArray, bBlock, nStart, nCount )
   LOCAL nLast, i
   IF ValType( bBlock ) != "B" .OR. ( ValType( aArray ) != "A" .AND. ! ( ValType( aArray ) == "O" .AND. ;
         __IsCollection( aArray ) ) )
      RETURN __ArgError( 2017 )
   ENDIF
   IF ValType( aArray ) == "O"
      VisitRange( aArray, nStart, nCount, {| x, i | Eval( bBlock, x, i ), .F. } )
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
      IF ! __IsCollection( aTarget )
         RETURN 0
      ENDIF
      IF lBlock // only .T. finds
         RETURN VisitRange( aTarget, nStart, nCount, {| x, i | ValType( x := Eval( xSearch, x, i ) ) == "L" .AND. x } )
      ENDIF
      RETURN VisitRange( aTarget, nStart, nCount, {| x | __ValueMatches( x, xSearch ) } )
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
      RETURN __ScanValue( aTarget, xSearch, nStart, nCount )
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

// HScan( hHash, xSearch, [nStart], [nCount] ): the position of the first entry of the range whose value matches xSearch
// (as AScan matches, __ScanValue), or for which the block xSearch( key, value, position ) returns .T.; 0 when none
// does, or without xSearch. The range is fixed when it starts: entries a block deletes end it. A hash is required
// (BASE/1123).
FUNCTION HScan( hHash, xSearch, nStart, nCount )
   LOCAL nLast, nPos := 0, k, v, x
   IF ValType( hHash ) != "H"
      RETURN __ArgError( 1123 )
   ENDIF
   IF PCount() < 2
      RETURN 0
   ENDIF
   nStart := __RangeStart( Len( hHash ), nStart )
   IF nStart == 0
      RETURN 0
   ENDIF
   nCount := __RangeCount( Len( hHash ) - nStart + 1, nCount )
   IF ValType( xSearch ) != "B"
      RETURN __ScanValue( hHash, xSearch, nStart, nCount )
   ENDIF
   nLast := nStart + nCount - 1
   FOR EACH k => v IN hHash
      IF ++nPos > nLast
         EXIT
      ENDIF
      IF nPos >= nStart .AND. ValType( x := Eval( xSearch, k, v, nPos ) ) == "L" .AND. x
         RETURN nPos
      ENDIF
   NEXT
RETURN 0

// HMerge( hDest, hSource, [bFilter] ): adds the entries of hSource to hDest (a key already there takes the new value),
// those for which bFilter( key, value, position ) returns .T. when given: the entry as it was before the block ran,
// whatever the block does with its arguments. Each position is checked against hSource as it is, so the entries a
// block adds are reached. Returns hDest; both must be hashes (BASE/1123).
FUNCTION HMerge( hDest, hSource, bFilter )
   LOCAL nPos := 0, k, v, x, xKey, xValue
   IF ValType( hDest ) != "H" .OR. ValType( hSource ) != "H"
      RETURN __ArgError( 1123 )
   ENDIF
   IF ValType( bFilter ) != "B"
      RETURN HCopy( hSource, hDest )
   ENDIF
   FOR EACH k => v IN hSource
      xKey := k // v designates the entry at the position, which the block may change or delete
      xValue := v
      x := Eval( bFilter, xKey, xValue, ++nPos )
      IF ValType( x ) == "L" .AND. x
         hDest[ xKey ] := xValue
      ENDIF
   NEXT
RETURN hDest
