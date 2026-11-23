// Array literals made by compiled code in the thread's hole when their form is known (an empty one, one with an
// element that is surely no NIL, logical or integer): each is a new array, empty or holding its elements, kept or
// dropped through young and full collections; a literal of integers or logicals (a compact form), one too large for
// the hole (15 items; 14 fit) and one built from `...` go through the runtime. A class whose objects copy only empty
// arrays (INIT {}, several, with other variables) gives each new object its own, made with it, also in a subclass, on
// several threads, grown past their room after collections; while an INIT value the program still holds is filled, the
// runtime copies what it holds; a class with an INIT of elements goes through the runtime.
STATIC s_aShared := {}

PROCEDURE Main()
   LOCAL aKeep := {}, i, a, o, n := 0, k, aTasks

   FOR i := 1 TO 100000
      a := {}
      AAdd( a, i )
      IF i % 1000 == 0
         AAdd( aKeep, a )
         AAdd( aKeep, { "s" + LTrim( Str( i ) ), a } )
      ENDIF
      IF i % 25000 == 0
         __GcCollect( i % 50000 != 0 )
      ENDIF
   NEXT
   FOR i := 1 TO Len( aKeep ) STEP 2
      n += aKeep[ i ][ 1 ] + Len( aKeep[ i + 1 ][ 1 ] ) + aKeep[ i + 1 ][ 2 ][ 1 ]
   NEXT
   ? "kept:", Len( aKeep ), n, aKeep[ 2 ][ 1 ], aKeep[ 2 ][ 2 ] == aKeep[ 1 ]

   a := { 1, 2, 3 }
   a[ 2 ] := "x"
   ? "integers:", a[ 1 ], a[ 2 ], a[ 3 ], Len( { .T., .F. } ), { NIL, 1 }[ 2 ]
   a := { 1.5, 2 }
   a[ 2 ] := a[ 2 ] + 1
   ? "mixed:", a[ 1 ], a[ 2 ], { "a", 1 }[ 2 ], Len( { "a", 1, .T., NIL } )
   a := { "a", 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 }
   ? "14 items:", Len( a ), a[ 1 ], a[ 14 ]
   a := { 1.5, "b", Date() == Date(), NIL, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }
   ? "large:", Len( a ), a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 18 ]
   ? "rest:", Len( Listed( "a", "b", "c" ) ), Listed( 1, "z" )[ 2 ]

   o := TBag():New()
   AAdd( o:aItems, 1 )
   AAdd( o:aMore, 2 )
   AAdd( o:aMore, 3 )
   ? "INIT {}:", Len( o:aItems ), Len( o:aMore ), Len( TBag():New():aItems ), o:aItems == TBag():New():aItems, o:cName
   aKeep := {}
   FOR i := 1 TO 3000
      o := TBag():New()
      AAdd( aKeep, o )
      IF i % 1000 == 0
         __GcCollect( i != 3000 )
      ENDIF
   NEXT
   FOR EACH o IN aKeep
      FOR k := 1 TO 40
         AAdd( o:aItems, k )
      NEXT
   NEXT
   ? "grown after collections:", Len( aKeep[ 1 ]:aItems ), aKeep[ 3000 ]:aItems[ 40 ], Len( aKeep[ 2 ]:aMore )
   o := TWideBag():New()
   AAdd( o:a8, "eight" )
   ? "object and arrays past 256 bytes:", Len( o:a1 ), o:a8[ 1 ], o:v9, Len( TWideBag():New():a8 )
   o := TSubBag():New()
   AAdd( o:aOwn, 1 )
   ? "subclass:", Len( o:aItems ), Len( o:aOwn ), Len( TSubBag():New():aOwn ), o:ClassName()

   o := THeld():New()
   AAdd( s_aShared, "held" )
   ? "INIT held and filled:", Len( o:a ), Len( THeld():New():a ), THeld():New():a[ 1 ], ;
      THeld():New():a == s_aShared

   o := TPair():New()
   AAdd( o:aPair, 3 )
   ? "INIT of elements:", Len( o:aPair ), Len( TPair():New():aPair )

   aTasks := {}
   FOR k := 1 TO 4
      AAdd( aTasks, TaskRun( {|| Bags( 20000 ) } ) )
   NEXT
   n := 0
   FOR EACH o IN aTasks
      n += o:Result
   NEXT
   ? "threads:", n
RETURN

STATIC FUNCTION Bags( nCount )
   LOCAL i, o, n := 0
   FOR i := 1 TO nCount
      o := TBag():New()
      AAdd( o:aItems, i )
      n += Len( o:aItems ) + Len( o:aMore )
   NEXT
RETURN n

STATIC FUNCTION Listed( ... )
RETURN { "first", ... }

STATIC FUNCTION Shared()
RETURN s_aShared

CLASS TBag
   VAR aItems INIT {}
   VAR nCount INIT 0
   VAR aMore INIT {}
   VAR cName INIT "bag"
ENDCLASS

CLASS TSubBag INHERIT TBag
   VAR aOwn INIT {}
ENDCLASS

CLASS TWideBag
   VAR a1 INIT {}
   VAR a2 INIT {}
   VAR a3 INIT {}
   VAR a4 INIT {}
   VAR a5 INIT {}
   VAR a6 INIT {}
   VAR a7 INIT {}
   VAR a8 INIT {}
   VAR v9 INIT 9
ENDCLASS

CLASS THeld
   VAR a INIT Shared()
ENDCLASS

CLASS TPair
   VAR aPair INIT { 1, 2 }
ENDCLASS
