// A parameter holds its argument until its function returns, inlined or not, and Self its object: an object is not
// destroyed while a function it was passed to, or its method, runs; `p := NIL` on the parameter lets go of it there.
// Each case makes an object whose destructor marks a log, in a function never inlined (BEGIN SEQUENCE) through a macro
// (its class unknown where it is used), and checks the log after a collection, at -O0 and -O2 (run0.*): passed on as
// the last thing an inlined function does, to a function that drops it; a parameter set to NIL that the function never
// reads (a typed copy at -O2, and a method's); an inlined function's parameter that an inner inlined function drops,
// and one value passed twice with one of the two dropped; a STATIC passed and then set to NIL by the function; an
// argument carried by a loop around an inlined call that makes another object; and a function that reads PCount()
// and drops its parameters (it keeps its arguments as received, as when called). Then what an inlined body held is
// let go after it: its result passed last to a function that drops it; and a number parameter set to NIL in a typed
// copy whose LOCAL keeps the value. And a parameter copied into a LOCAL, both set to NIL in turn (a method, and an
// inlined function with a loop): the object stays while the copy holds it, and goes after; and a parameter set to NIL
// once its copy is passed on to an inlined function, which holds it. (A parameter set to NIL on some paths only, not
// on every path to where its copy dies, holds the object until the function returns.) And an array literal passed to
// an inlined function that never reads it, or reads its element into a LOCAL it then sets to NIL: the array holds the
// object until the function returns.
STATIC s_o
PROCEDURE Main()
   LOCAL aLog := { .F. }, aSeen := {}, x, i, o
   ? "passed on last:", Passes( Made( aLog ), aLog )
   aLog := { .F. }
   ? "unread, typed:", Unread( Made( aLog ), 3, aLog ), "method:", Made( { .F. } ):Drop( Made( aLog := { .F. } ), aLog )
   aLog := { .F. }
   ? "outer holds:", Outer( Made( aLog ), aLog )
   aLog := { .F. }
   o := Made( aLog )
   ? "passed twice:", Twice( o, o, aLog )
   o := NIL
   aLog := { .F. }
   s_o := Made( aLog )
   ? "a STATIC:", StaticDropped( s_o, aLog )
   aLog := { .F. }
   x := Made( aLog )
   FOR i := 1 TO 2
      x := Carried( x, aLog, aSeen )
   NEXT
   ? "carried by a loop:", aSeen[ 1 ], aSeen[ 2 ]
   aLog := { .F. }
   ? "PCount():", Counted( Made( aLog ), { 1 }, aLog )
   aLog := { .F. }
   ? "after an inlined body:", Drops( Shown( Made( aLog ) ), aLog ), "number kept:", Kept( 5 )
   aLog := { .F. }
   ? "copied, both dropped:", Made( { .F. } ):Copied( Made( aLog ), aLog ), ;
      CopiedInlined( Made( aLog := { .F. } ), aLog )
   aLog := { .F. }
   ? "parameter dropped, its copy passed on:", Made( { .F. } ):PassesCopy( Made( aLog ), aLog )
   aLog := { .F. }
   ? "array unread:", ArrayUnread( { Made( aLog ) }, aLog )
   aLog := { .F. }
   ? "array element dropped:", ArrayElementDropped( { Made( aLog ) }, aLog )
RETURN

STATIC FUNCTION ArrayUnread( v, aLog )
   LOCAL x := 1
   x := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

STATIC FUNCTION ArrayElementDropped( v, aLog )
   LOCAL y := v[ 1 ], x := 1
   y := NIL
   x := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

STATIC FUNCTION Passes( o, aLog )
RETURN Drops( o, aLog )

STATIC FUNCTION Drops( p, aLog )
   LOCAL c
   BEGIN SEQUENCE
      p := NIL
      __GcCollect()
      c := IIf( aLog[ 1 ], "destroyed", "kept" )
   END SEQUENCE
RETURN c

STATIC FUNCTION Unread( o, n, aLog )
   o := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "freed", "kept" ) + Str( n, 2 )

STATIC FUNCTION Outer( o, aLog )
   LOCAL c := Inner( o, aLog )
RETURN c

STATIC FUNCTION Inner( p, aLog )
   p := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

STATIC FUNCTION Twice( a, b, aLog )
   a := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

STATIC FUNCTION StaticDropped( p, aLog )
   LOCAL z := { 1 }
   s_o := NIL
   ?? ""
   z := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

STATIC FUNCTION Carried( p, aLog, aSeen )
   LOCAL q := Made( { .F. } )
   __GcCollect()
   AAdd( aSeen, aLog[ 1 ] ) // straight on: the object made is passed on the loop's back edge from this block
RETURN q

STATIC FUNCTION Counted( o, b, aLog )
   b := NIL
   o := NIL
   __GcCollect()
RETURN Str( PCount(), 1 ) + IIf( aLog[ 1 ], "destroyed", "kept" )

// An object of class Logged marking aLog when destroyed.
STATIC FUNCTION Made( aLog )
   LOCAL o
   BEGIN SEQUENCE
      o := &( "Logged():New()" )
      o:aLog := aLog
   END SEQUENCE
RETURN o

CLASS Logged
   VAR aLog
   METHOD New() INLINE Self
   METHOD Drop( p, aLog )
   METHOD Copied( p, aLog )
   METHOD PassesCopy( p, aLog )
   DESTRUCTOR Done
ENDCLASS

METHOD Drop( p, aLog ) CLASS Logged
   p := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "freed", "kept" )

METHOD Copied( p, aLog ) CLASS Logged
   LOCAL q := p, c
   p := NIL
   __GcCollect()
   c := IIf( aLog[ 1 ], "destroyed", "kept" ) + ValType( q )
   q := NIL
   __GcCollect()
RETURN c + IIf( aLog[ 1 ], " then destroyed", " then kept" )

METHOD PassesCopy( p, aLog ) CLASS Logged
   LOCAL q := p
   p := NIL
RETURN Holds( q, aLog )

METHOD PROCEDURE Done CLASS Logged
   ::aLog[ 1 ] := .T.
RETURN

// Inlined, with a frame of its own (it calls QOut()): holds p while it runs.
STATIC FUNCTION Shown( p )
   QOut( "shown" )
RETURN p

// A typed copy at -O2 (its parameter a number): m keeps the value read from the parameter's slot after n := NIL.
STATIC FUNCTION Kept( n )
   LOCAL m := n, i, c := ""
   n := NIL
   FOR i := 1 TO 3
      c += Str( i, 1 ) + Str( i * 2, 1 ) + Str( i * 3, 1 ) + Str( i * 4, 1 ) + Str( i * 5, 1 ) + Str( i * 6, 1 )
      c += Str( i * 7, 1 ) + Str( i * 8, 2 ) + Str( i * 9, 2 ) + Str( i * 10, 2 ) + Str( i * 11, 2 )
   NEXT
RETURN LTrim( Str( m ) ) + " " + c

STATIC FUNCTION CopiedInlined( p, aLog )
   LOCAL q := p, c, i, x := ""
   p := NIL
   __GcCollect()
   c := IIf( aLog[ 1 ], "destroyed", "kept" ) + ValType( q )
   q := NIL
   __GcCollect()
   FOR i := 1 TO 3
      x += "x"
   NEXT
RETURN c + IIf( aLog[ 1 ], " then destroyed", " then kept" ) + x

// Inlined: o is held while it runs, though nothing reads it.
STATIC FUNCTION Holds( o, aLog )
   LOCAL z := { 1 }
   ?? ""
   z := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )
