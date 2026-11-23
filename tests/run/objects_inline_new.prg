// Objects made by compiled code in the thread's hole (classes whose objects take nothing but their memory, a subclass
// of a class of another module too): their variables start as the class's initial values (numbers, a string, NIL, an
// inherited one), each object its own; they live through young and full collections (moved, aged), kept in an array
// or dropped; a class without variables; a class whose objects copy only an empty array (INIT {}), made with them; a
// class with a destructor, an ABSTRACT one and one too large for the hole go through the runtime as before; objects
// made on several threads at once.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL aKeep := {}, i, o, n := 0, oErr, a, k

   FOR i := 1 TO 200000
      o := TPoint():New( i, -i )
      IF i % 1000 == 0
         AAdd( aKeep, o )
      ENDIF
      IF i % 50000 == 0
         __GcCollect( i % 100000 != 0 )
      ENDIF
   NEXT
   FOR EACH o IN aKeep
      n += o:x + o:y + Len( o:cName ) + IIf( o:xTag == NIL, 1, 0 )
   NEXT
   ? "kept:", Len( aKeep ), n, aKeep[ 1 ]:cName, aKeep[ 200 ]:x
   aKeep[ 1 ]:cName := "changed"
   ? "own copy:", aKeep[ 1 ]:cName, aKeep[ 2 ]:cName, TPoint():New( 0, 0 ):cName

   o := T3D():New( 1, 2 )
   ? "subclass:", o:x, o:y, o:z, o:cName, o:ClassName()

   o := TListed():New()
   AAdd( o:aItems, 1 )
   ? "copied INIT:", Len( o:aItems ), Len( TListed():New():aItems )

   s_nGone := 0
   FOR i := 1 TO 1000
      o := TGone():New()
   NEXT
   o := NIL
   __GcCollect()
   ? "destructors ran:", s_nGone > 0

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      o := TShape():New()
   RECOVER USING oErr
      ? "abstract:", oErr:description
   END SEQUENCE

   o := TLocal():New()
   ? "other module's parent:", o:ClassName(), o:cFrom, o:nOwn, TLocal():New():cFrom

   o := TEmpty():New()
   ? "no variables:", o:ClassName(), ValType( o ), TEmpty():New():ClassName()

   aKeep := {}
   FOR i := 1 TO 1000
      AAdd( aKeep, TWide():New() )
   NEXT
   aKeep[ 500 ]:v16 := "set"
   __GcCollect()
   ? "too large for the hole:", aKeep[ 1 ]:v1, aKeep[ 1000 ]:v16, aKeep[ 500 ]:v16

   a := {}
   FOR k := 1 TO 4
      AAdd( a, TaskRun( {|| Made( 50000 ) } ) )
   NEXT
   n := 0
   FOR EACH o IN a
      n += o:Result
   NEXT
   ? "threads:", n
RETURN

STATIC FUNCTION Made( nCount )
   LOCAL i, o, n := 0
   FOR i := 1 TO nCount
      o := TPoint():New( i, 1 )
      n += o:y
   NEXT
RETURN n

PROCEDURE Gone()
   s_nGone++
RETURN

CLASS TPoint
   VAR x INIT 0
   VAR y INIT 0
   VAR cName INIT "point"
   VAR xTag
   METHOD New( x, y ) INLINE ::x := x, ::y := y, Self
ENDCLASS

CLASS T3D INHERIT TPoint
   VAR z INIT 7
ENDCLASS

CLASS TListed
   VAR aItems INIT {}
ENDCLASS

CLASS TGone
   DESTRUCTOR Bye()
ENDCLASS

METHOD PROCEDURE Bye CLASS TGone
   Gone()
RETURN

ABSTRACT CLASS TShape
ENDCLASS

CLASS TLocal INHERIT TRemote
   VAR nOwn INIT 3
ENDCLASS

CLASS TEmpty
ENDCLASS

CLASS TWide
   VAR v1 INIT 1
   VAR v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
   VAR v16 INIT "last"
ENDCLASS
