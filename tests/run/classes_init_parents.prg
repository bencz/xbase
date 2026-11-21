// Constructors with parameters under multiple inheritance, at -O0 and -O2 (run0.*): New( args ) runs the class's Init
// with them; a class with several parents calls each parent's Init itself, ::Super:Init( ... ) for the first parent and
// ::TParent:Init( ... ) (a cast) for the others, each with its own arguments; a class without Init runs the first
// Init its line finds (the first parent's), the other parents' variables keeping their INIT values; an Init that calls
// its own parent's (a chain through grandparents); a diamond, whose shared ancestor's Init each side calls; fewer
// arguments than parameters (NIL), PCount() in Init, and an Init that returns something else than Self (New gives the
// object all the same). The same as the reference, but PCount() in an INLINE method: the arguments after Self, as in
// any method (deliberately: the reference counts Self there, and only there).
PROCEDURE Main()
   LOCAL o
   o := TChild():New( "a1", 2, { 3 } )
   ? "three parents:", o:a, o:b, Len( o:c ), o:own, o:TA:a, o:TB:b
   o := TNoInit():New( "only a" )
   ? "no Init of its own:", o:a, o:b, o:c
   o := TGrand():New( 7 )
   ? "chain:", o:g, o:p, o:k, o:trace
   o := TDiamond():New( 10, 20 )
   ? "diamond:", o:l, o:r, o:TLeft:base, o:TRight:base, o:calls
   o := TChild():New( "x" )
   ? "fewer arguments:", o:a, o:b, o:c, o:own
   o := TCounts():New( 1, , 3 )
   ? "pcount:", o:n, ValType( o ), o:ClassName()
RETURN

CLASS TA
   VAR a INIT "a?"
   METHOD Init( x ) INLINE ::a := x, Self
ENDCLASS

CLASS TB
   VAR b INIT "b?"
   METHOD Init( x ) INLINE ::b := x, Self
ENDCLASS

CLASS TC
   VAR c INIT {}
   METHOD Init( x ) INLINE ::c := x, Self
ENDCLASS

CLASS TChild INHERIT TA, TB, TC
   VAR own INIT ""
   METHOD Init( x, y, z )
ENDCLASS

METHOD Init( x, y, z ) CLASS TChild
   ::Super:Init( x )
   ::TB:Init( y )
   ::TC:Init( IIf( z == NIL, { "default" }, z ) )
   ::own := ValType( x ) + ValType( y ) + ValType( z )
RETURN Self

CLASS TNoInit INHERIT TA, TB, TC
ENDCLASS

CLASS TParentOf
   VAR p INIT 0
   VAR trace INIT ""
   METHOD Init( n ) INLINE ::p := n * 10, ::trace += "P", Self
ENDCLASS

CLASS TMiddle INHERIT TParentOf
   VAR k INIT 0
   METHOD Init( n ) INLINE ::k := n + 1, ::trace += "M", ::Super:Init( n ), Self
ENDCLASS

CLASS TGrand INHERIT TMiddle
   VAR g INIT 0
   METHOD Init( n ) INLINE ::g := n, ::trace += "G", ::Super:Init( n * 2 ), Self
ENDCLASS

CLASS TBase
   VAR base INIT 0
   VAR calls INIT 0
   METHOD Init( n ) INLINE ::base := n, ::calls++, Self
ENDCLASS

CLASS TLeft INHERIT TBase
   VAR l INIT 0
   METHOD Init( n ) INLINE ::l := n, ::Super:Init( n + 1 ), Self
ENDCLASS

CLASS TRight INHERIT TBase
   VAR r INIT 0
   METHOD Init( n ) INLINE ::r := n, ::Super:Init( n + 2 ), Self
ENDCLASS

CLASS TDiamond INHERIT TLeft, TRight
   METHOD Init( a, b ) INLINE ::TLeft:Init( a ), ::TRight:Init( b ), Self
ENDCLASS

CLASS TCounts
   VAR n INIT 0
   METHOD Init( a, b, c ) INLINE ::n := PCount(), 42
ENDCLASS
