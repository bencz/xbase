// A loop that makes calls clears, where it is entered, the slots of the results of calls made before it that it no
// longer needs, but not what an inlined body around it still holds: the argument group of a body that reads its
// arguments as received (PCount()), and the argument of a body whose loop starts with another inlined call, read after
// the loop through its parameter. In both, the object passed (whose destructor counts) lives through the collections
// the loop makes, at every level.
STATIC s_nGone := 0

PROCEDURE Main()
   ? "read as received:", Counted( Make() ), s_nGone
   __GcCollect() // the first object goes now
   s_nGone := 0
   ? "held by the body:", Held( Made() ), s_nGone
RETURN

STATIC FUNCTION Counted( o )
   LOCAL n := 0
   DO WHILE n < 3
      n++
      __GcCollect()
   ENDDO
RETURN LTrim( Str( PCount() ) ) + "/" + LTrim( Str( n ) ) + "/" + LTrim( Str( s_nGone ) )

STATIC FUNCTION Held( o )
   LOCAL n := 0
   DO WHILE Small( n )
      n++
      __GcCollect()
   ENDDO
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( s_nGone ) ) + "/" + LTrim( Str( o:nId ) )

STATIC FUNCTION Small( n )
RETURN n < 3

STATIC FUNCTION Make()
   LOCAL o := TGone():New()
   o:nId := Seconds()
RETURN o

// Not inlined: a sequence.
STATIC FUNCTION Made()
   LOCAL o
   BEGIN SEQUENCE
      o := TGone():New()
      o:nId := 7
   END SEQUENCE
RETURN o

PROCEDURE Gone()
   s_nGone++
RETURN

CLASS TGone
   VAR nId INIT 0
   DESTRUCTOR Bye()
ENDCLASS

METHOD PROCEDURE Bye CLASS TGone
   Gone()
RETURN
