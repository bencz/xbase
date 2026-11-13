// The functions over a range of an array, a hash or a collection object (AEval, AScan, AFill, ACopy, HEval, HScan,
// HMerge), with the collection functions' rule for the range (__RangeStart, __RangeCount). The compiler also reads
// this source to run calls of them in place (docs/xir.md, Inlining).

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
// without it; none when it is 0; a negative count, as in Clipper, is the rest from the first element and nothing from
// another) takes the value; without a value nothing changes. Returns aTarget; anything but an array is BASE/6004.
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
      IF Int( nCount ) == 0 .OR. ( Int( nCount ) < 0 .AND. nStart != 1 )
         RETURN aTarget
      ENDIF
      IF Int( nCount ) > 0
         nLast := Min( nLast, nStart + Int( nCount ) - 1 )
      ENDIF
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
