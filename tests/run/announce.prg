// ANNOUNCE names a public procedure of the module: an empty one, which returns NIL, unless the module defines it, and
// Do() finds it by name. EXTERNAL (REQUEST in std.ch) names functions to link in: the module's own, the runtime's and
// another module's, at file level or in a function, abbreviated or not; ANNOUNCE and EXTERNAL stay usable as names.
ANNOUNCE AnnouncedMod
REQUEST LibMod, Str
EXTE Twice

PROCEDURE Main()
   LOCAL external := 2, announce := 3
   EXTERNAL Upper
   REQUEST Twice, LibFn
   ? ValType( AnnouncedMod() ), AnnouncedMod() == NIL, ValType( Do( "ANNOUNCEDMOD" ) )
   ? external + announce, Twice( 21 )
   ? Do( "LIBFN" ), ValType( Do( "LIBMOD" ) )
RETURN

STATIC FUNCTION Twice( n )
RETURN n * 2
