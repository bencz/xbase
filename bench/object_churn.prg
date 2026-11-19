// Objects replaced in a graph: 50,000 slots holding objects of three compiled classes that point to other slots'
// objects (cycles included); 700,000 times a slot's object is replaced by a new one, then the graph is walked.
STATIC s_nSeed := 9

PROCEDURE Main()
   LOCAL t := ElapsedMs(), aSlots := Array( 50000 ), i, k, nKind, o, n := 0, ok
   FOR i := 1 TO Len( aSlots )
      aSlots[ i ] := TLeaf():New( i )
   NEXT
   FOR i := 1 TO 700000
      k := Rand( Len( aSlots ) ) + 1
      nKind := Rand( 3 )
      DO CASE
      CASE nKind == 0
         o := TLeaf():New( i % 100 )
      CASE nKind == 1
         o := TPair():New( i % 100, aSlots[ Rand( Len( aSlots ) ) + 1 ], aSlots[ Rand( Len( aSlots ) ) + 1 ] )
      OTHERWISE
         o := TList():New( i % 100, aSlots[ Rand( Len( aSlots ) ) + 1 ] )
      ENDCASE
      aSlots[ k ] := o
   NEXT
   FOR EACH o IN aSlots
      n += o:Weight()
   NEXT
   ok := n == 5162688
   ? "object_churn", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

CLASS TLeaf
   VAR nValue INIT 0
   METHOD New( n ) INLINE ::nValue := n, Self
   METHOD Weight() INLINE ::nValue
ENDCLASS

CLASS TPair INHERIT TLeaf
   VAR oLeft
   VAR oRight
   METHOD New( n, oL, oR ) INLINE ::nValue := n, ::oLeft := oL, ::oRight := oR, Self
   METHOD Weight() INLINE ::nValue + ::oLeft:nValue + ::oRight:nValue
ENDCLASS

CLASS TList INHERIT TLeaf
   VAR aItems
   METHOD New( n, o ) INLINE ::nValue := n, ::aItems := { o, n, Str( n ) }, Self
   METHOD Weight() INLINE ::nValue + ::aItems[ 1 ]:nValue + Len( ::aItems[ 3 ] )
ENDCLASS

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
