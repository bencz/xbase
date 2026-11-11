// Class variables and friends, compared with the reference: CLASSVAR, CLASS VAR and CLASSDATA (INIT or ASSIGN), one
// value per class (a subclass starts from the INIT value, not the parent's current one; an INIT array is the same array
// for both), SHARED ones common to the class and its subclasses, READONLY assigned from outside, and FRIEND CLASS (its
// own methods, not its subclasses') and FRIEND FUNCTION (with the codeblocks it makes and the macros it evaluates)
// reaching HIDDEN and PROTECTED members and assigning READONLY ones. Also: ObjHasVar and ObjVars with class variables,
// and a class variable through a cast.
PROCEDURE Main()
   LOCAL a := TA():New(), b := TB():New(), e
   ? "start:", a:count, b:count, a:shared, b:shared, a:list == b:list
   a:Inc()
   ? "a inc:", a:count, b:count, a:shared, b:shared
   b:Inc()
   b:Inc()
   ? "b inc:", a:count, b:count, a:shared, b:shared, TA():New():count, TC():New():count, TC():New():shared
   AAdd( a:list, 1 )
   ? "list:", Len( a:list ), Len( b:list ), Len( TA():New():list )
   ? "cast:", b:TA:count, ObjHasVar( a, "count" ), ArrayText( ObjVars( a ) )
   ? "friends:", Peek( TF():New() ), TFriend():New():Peek( TF():New() ), Eval( Blk(), TF():New() ), ;
      PeekMacro( TF():New() )

   ErrorBlock( {| oErr | QOut( "error:", oErr:subCode, oErr:description, oErr:operation ), Break( oErr ) } )
   FOR EACH e IN { {|| a:ro := 1 }, {|| TStranger():New():Peek( TF():New() ) }, {|| NotFriend( TF():New() ) }, ;
                   {|| TFriendChild():New():Peek( TF():New() ) } }
      BEGIN SEQUENCE
         Eval( e )
      END SEQUENCE
   NEXT
RETURN

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", s
   FOR EACH s IN a
      c += IIF( Empty( c ), "", "," ) + s
   NEXT
RETURN c

FUNCTION Peek( o )
   o:ro := 9
RETURN o:h + o:p + o:ro

FUNCTION PeekMacro( o )
   LOCAL c := "oP:h"
   PRIVATE oP := o
RETURN &c

FUNCTION Blk()
RETURN {| o | o:h }

FUNCTION NotFriend( o )
RETURN o:h

CLASS TA
   VAR plain INIT 0
   CLASSVAR count INIT 10
   CLASS VAR shared ASSIGN 100 SHARED
   CLASSDATA list INIT {}
   CLASSVAR ro INIT 5 READONLY
   METHOD Inc() INLINE ::count++, ::shared++
ENDCLASS

CLASS TB INHERIT TA
ENDCLASS

CLASS TC INHERIT TB
ENDCLASS

CLASS TF
   VAR ro INIT 1 READONLY
   HIDDEN:
   VAR h INIT 10
   PROTECTED:
   VAR p INIT 100
   FRIEND CLASS TFriend
   FRIEND FUNCTION Peek, Blk, PeekMacro
ENDCLASS

CLASS TFriend
   METHOD Peek( o ) INLINE o:h + o:p
ENDCLASS

CLASS TFriendChild INHERIT TFriend
   METHOD Peek( o ) INLINE o:h + o:p
ENDCLASS

CLASS TStranger
   METHOD Peek( o ) INLINE o:h
ENDCLASS
