// Array functions written in xBase (the others are in rtl/arrays.c).

// AEval( aArray, bBlock, [nStart], [nCount] ): calls bBlock( element, position ) for each element of the range (the
// collection functions' rule: __RangeStart, __RangeCount) that still exists when its turn comes; returns the array.
FUNCTION AEval( aArray, bBlock, nStart, nCount )
   LOCAL nLast, i
   IF ValType( aArray ) != "A" .OR. ValType( bBlock ) != "B"
      RETURN __ArgError( 2017 )
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
