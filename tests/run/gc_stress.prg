// The collector moving objects: a heap fragmented on purpose (small and medium objects interleaved, every other one
// dropped while more are made) so that full collections empty blocks, moving what lives there, and young ones sweep
// the size classes' slots; then every structure is read back: strings of every size, arrays with items of their own
// and grown ones, hashes (their tables move with their entries), codeblocks and the cells they share with the
// function, objects, references of FOR EACH, and copies made by AClone while collections run.
STATIC s_nSeed := 7

CLASS Pair
   VAR cName
   VAR aList
   METHOD New( cName, aList ) CONSTRUCTOR
   METHOD Sum()
ENDCLASS

METHOD New( cName, aList ) CLASS Pair
   ::cName := cName
   ::aList := aList
RETURN Self

METHOD Sum() CLASS Pair
   LOCAL n := 0, x
   FOR EACH x IN ::aList
      n += Len( x )
   NEXT
RETURN n

PROCEDURE Main()
   LOCAL aKeep := {}, aJunk, hTable := { => }, aBlocks := {}, aObjects := {}, aClones := {}, i, j, x, n, nBad := 0
   LOCAL nCounter := 0, bCount := {|| ++nCounter }
   FOR j := 1 TO 6
      aJunk := {}
      FOR i := 1 TO 8000
         x := Text( i + j * 100000 )
         AAdd( aJunk, x )
         IF i % 2 == 0
            AAdd( aKeep, x )
            hTable[ "k" + LTrim( Str( Len( aKeep ) ) ) ] := x
         ENDIF
         IF i % 997 == 0
            AAdd( aBlocks, { MakeBlock( Len( aKeep ) ), Len( aKeep ) * 2 } )
            AAdd( aObjects, Pair():New( x, { x, Text( i ) } ) )
            AAdd( aClones, AClone( { x, { Text( i ), aKeep[ Len( aKeep ) ] } } ) )
            Eval( bCount )
         ENDIF
      NEXT
      aJunk := NIL
      __GcCollect( .T. )
   NEXT
   __GcCollect()
   FOR i := 1 TO Len( aKeep )
      IF ! hTable[ "k" + LTrim( Str( i ) ) ] == aKeep[ i ] .OR. ! Valid( aKeep[ i ] )
         nBad++
      ENDIF
   NEXT
   FOR EACH x IN aBlocks
      IF ! Eval( x[ 1 ] ) == x[ 2 ]
         nBad++
      ENDIF
   NEXT
   n := 0
   FOR EACH x IN aObjects
      n += x:Sum()
      IF ! x:cName == x:aList[ 1 ]
         nBad++
      ENDIF
   NEXT
   FOR EACH x IN aClones
      IF ! Valid( x[ 1 ] ) .OR. ! Valid( x[ 2, 1 ] ) .OR. ! Valid( x[ 2, 2 ] )
         nBad++
      ENDIF
   NEXT
   FOR EACH x IN aKeep // a reference to each element, written through
      x := Left( x, 3 ) + SubStr( x, 4 )
   NEXT
   FOR i := 1 TO Len( aKeep )
      IF ! Valid( aKeep[ i ] )
         nBad++
      ENDIF
   NEXT
   ? "kept:", Len( aKeep ), "table:", Len( hTable ), "blocks:", Len( aBlocks ), "objects:", Len( aObjects ), ;
      "sum ok:", n > 0, "counter:", nCounter, "bad:", nBad
RETURN

// A string of a size from 12 to 711 bytes that tells its own length and number.
STATIC FUNCTION Text( n )
   LOCAL nLen := 12 + Rand( 700 )
RETURN PadR( StrZero( nLen, 4 ) + StrZero( n, 7 ) + "|", nLen, Chr( 65 + n % 26 ) )

// Whether a string made by Text() is whole.
STATIC FUNCTION Valid( c )
   LOCAL nLen := Val( Left( c, 4 ) ), n := Val( SubStr( c, 5, 7 ) )
RETURN Len( c ) == nLen .AND. SubStr( c, 12, 1 ) == "|" .AND. ;
   Right( c, 1 ) == iif( nLen > 12, Chr( 65 + n % 26 ), "|" )

// A codeblock sharing a variable (in a cell) with the function that made it, which changes it after.
STATIC FUNCTION MakeBlock( n )
   LOCAL nValue := n, b := {|| nValue }
   nValue := n * 2
RETURN b

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 1103515245 + 12345 ) % 2147483648
RETURN Int( s_nSeed / 65536 ) % n
