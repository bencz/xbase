// A trie of hashes, one per node, from a letter to the node { hChildren, nWords }: 100,000 words of 3 to 10 letters
// from an 8-letter alphabet inserted, then 100,000 other words of the same kind looked up, as words and as prefixes
// of the words stored.
STATIC s_nSeed := 2718

PROCEDURE Main()
   LOCAL t, aWords := Array( 100000 ), aProbe := Array( 100000 ), aRoot := { { => }, 0 }, i, w, node, j, c, ok
   LOCAL nNodes := 1, nFound := 0, nPrefix := 0
   FOR i := 1 TO Len( aWords )
      aWords[ i ] := NewWord()
      aProbe[ i ] := NewWord()
   NEXT
   t := ElapsedMs()
   FOR EACH w IN aWords
      node := aRoot
      FOR j := 1 TO Len( w )
         c := SubStr( w, j, 1 )
         IF c $ node[ 1 ]
            node := node[ 1 ][ c ]
         ELSE
            node := ( node[ 1 ][ c ] := { { => }, 0 } )
            nNodes++
         ENDIF
      NEXT
      node[ 2 ]++
   NEXT
   FOR EACH w IN aProbe
      node := aRoot
      FOR j := 1 TO Len( w )
         c := SubStr( w, j, 1 )
         IF !( c $ node[ 1 ] )
            node := NIL
            EXIT
         ENDIF
         node := node[ 1 ][ c ]
      NEXT
      IF node != NIL
         nPrefix++
         IF node[ 2 ] > 0
            nFound++
         ENDIF
      ENDIF
   NEXT
   ok := nNodes == 159557 .AND. nFound == 40767 .AND. nPrefix == 50841
   ? "trie", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION NewWord()
   LOCAL n := Rand( 8 ) + 3, w := "", i
   FOR i := 1 TO n
      w += SubStr( "etaoinsr", Rand( Rand( 8 ) + 1 ) + 1, 1 )
   NEXT
   RETURN w

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
