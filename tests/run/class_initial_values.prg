// The variables a new object starts with: each class of the line's INIT values in their slots (a subclass's own after
// its parent's, a diamond's shared ancestor once), the same values for every object (numbers, strings, NIL), and an
// array or a hash a copy of its own, nested ones too (one object's changes reach no other, nor the next new one). A
// class whose INIT values failed is not usable until they are evaluated again and succeed (TFragile: its first use
// fails, its second gets them; deliberately: the reference leaves such a class unusable for good).
STATIC s_nTries := 0

PROCEDURE Main()
   LOCAL o1 := TLeaf():New(), o2 := TLeaf():New(), o3, oErr
   AAdd( o1:aList, 9 )
   AAdd( o1:aList[ 2 ], 8 )
   o1:hMap[ "k" ][ 1 ] := 7
   o1:hMap[ "new" ] := 1
   o3 := TLeaf():New()
   ? "first:", Len( o1:aList ), Len( o1:aList[ 2 ] ), o1:hMap[ "k" ][ 1 ], Len( o1:hMap )
   ? "second:", Len( o2:aList ), Len( o2:aList[ 2 ] ), o2:hMap[ "k" ][ 1 ], Len( o2:hMap )
   ? "third:", Len( o3:aList ), Len( o3:aList[ 2 ] ), o3:hMap[ "k" ][ 1 ], Len( o3:hMap )
   ? "plain:", o1:nBase, o1:cName, o1:xNone, o1:nLeft, o1:nRight, o1:nLeaf, o3:nBase, o3:cName
   ? "diamond:", o1:TLeft:nBase, o1:TRight:nBase, o1:TBase:cName
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      TFragile():New()
      ? "fragile: no error"
   RECOVER USING oErr
      ? "fragile, first:", oErr:Description
   END SEQUENCE
   ? "fragile, second:", TFragile():New():nValue, TFragile():New():aBox[ 1 ], s_nTries
RETURN

STATIC FUNCTION Fragile()
   IF ++s_nTries == 1
      RETURN 1 + "a"
   ENDIF
RETURN s_nTries * 10

CLASS TBase
   VAR nBase INIT 1
   VAR cName INIT "base"
   VAR xNone
   VAR aList INIT { 1, { 2 } }
   VAR hMap INIT { "k" => { 3 } }
ENDCLASS

CLASS TLeft INHERIT TBase
   VAR nLeft INIT 10
ENDCLASS

CLASS TRight INHERIT TBase
   VAR nRight INIT 20
ENDCLASS

CLASS TLeaf INHERIT TLeft, TRight
   VAR nLeaf INIT 30
ENDCLASS

CLASS TFragile
   VAR nValue INIT Fragile()
   VAR aBox INIT { "box" }
ENDCLASS
